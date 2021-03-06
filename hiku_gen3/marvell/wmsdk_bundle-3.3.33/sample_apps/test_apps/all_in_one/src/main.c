/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* WLAN Application using micro-AP and station
 *
 * Summary:
 *
 * This application showcases the end-to-end functionality of the SDK,
 * from configuring the device for the first time (provisioning),
 * to connection with the configured APs, to advertising mdns services,
 * to providing HTTP/HTML interfaces for interaction with the device,
 * to communicating with the cloud server.
 * This application also showcases the use of advanced features like
 * overlays, power management.
 *
 * Description:
 *
 * The application is written using Application Framework that
 * simplifies development of WLAN networking applications.
 *
 * WLAN Initialization:
 *
 * When the application framework is started, it starts up the WLAN
 * sub-system and initializes the network stack. The app receives the event
 * when the WLAN subsystem has been started and initialized.
 *
 * If the device is not provisioned, Application framework sends an event
 * to the app to start a micro-AP network along with a DHCP server. This
 * will create a WLAN network and also creates a IP network interface
 * associated with that WLAN network. The DHCP server allows devices
 * to associate and connect to this network and establish an IP level
 * connection.
 *
 * When the network is initialized and ready, app will receive a
 * Micro-AP started event.
 *
 * Micro-AP Started:
 *
 * The micro-AP network is now up and ready.
 *
 * The app starts the web-server which will then listen to incoming http
 * connections (e.g, from a browser).
 *
 * By default, device can be accessed at URI http://192.168.10.1
 * Through the Web UI, user can provision the device to an Access Point.
 * Once the device is provisioned successfully to the configured Access
 * Point, app will receive a CONNECTED event.
 *
 * CONNECTED:
 *
 * Now the station interface of the device is up and app takes the
 * Micro-AP interface down. App enables MC200 + WiFi power save mode
 *
 * Cloud support:
 * By default, cloud support is not active. User can activate it by
 * executing following commands on the prompt of the device:
 * psm-set cloud enabled 1
 * psm-set cloud url "http://<ip-address>/cloud"
 * After the device reboot, cloud will get activated.
 *
 */
#include <wm_os.h>
#include <app_framework.h>
#include <partition.h>
#include <appln_cb.h>
#include <appln_dbg.h>
#include <cli.h>
#include <wmstdio.h>
#include <wm_net.h>
#include <mdns_helper.h>
#include <wps_helper.h>
#include <reset_prov_helper.h>
#include <power_mgr_helper.h>
#include <httpd.h>
#include <wmcloud.h>
#include <led_indicator.h>
#include <board.h>
#include <dhcp-server.h>
#include <wmtime.h>
#include <psm.h>
#include <ftfs.h>
#include <rfget.h>
#include <diagnostics.h>
#include <mdev_gpio.h>
#include <healthmon.h>
#include <critical_error.h>
#include "aio_cloud.h"
#include "aio_wps_cli.h"
#include <aio_overlays.h>
#include "server-cert.pem.h"
#include "server-key.pem.h"
#include "aio_cloud.h"

#ifdef APPCONFIG_XIP_ENABLE
#include <cache_profile.h>
#include <wmsysinfo.h>
#include <boot_flags.h>
#endif

/*-----------------------Global declarations----------------------*/
static char g_uap_ssid[IEEEtypes_SSID_SIZE + 1];
appln_config_t appln_cfg = {
	.passphrase = "marvellwm",
	.wps_pb_gpio = -1,
	.reset_prov_pb_gpio = -1
};

int ftfs_api_version = 100;
char *ftfs_part_name = "ftfs";

struct fs *fs;

static os_timer_t uap_down_timer;

#define UAP_DOWN_TIMEOUT (30 * 1000)
#define NUM_TOKENS	20

#ifdef APPCONFIG_HTTPS_SERVER
#define EPOCH_TIME 1388534400 /* epoch value of 1 Jan 2014 00:00:00 */
#endif /* APPCONFIG_HTTPS_SERVER */

#define NETWORK_MOD_NAME	"network"
#define VAR_UAP_SSID		"uap_ssid"
#define VAR_PROV_KEY            "prov_key"

/** Provisioning done timer call back function
 * Once the provisioning is done, we wait for provisioning client to send
 * AF_EVT_PROV_CLIENT_DONE which stops uap and dhcp server. But if any case
 * client doesn't send AF_EVT_PROV_CLIENT_DONE event, then we wait for
 * 60seconds(timer) to shutdown UAP.
 */
static void uap_down_timer_cb(os_timer_arg_t arg)
{
	if (is_uap_started()) {
		hp_mdns_deannounce(net_get_uap_handle());
		app_uap_stop();
	}
}

/* This function initializes the SSID with the PSM variable network.uap_ssid
 * If the variable is not found, the default value is used.
 * To change the ssid, please set the network.uap_ssid variable
 * from the console.
 */
void appln_init_ssid()
{
	if (psm_get_single(NETWORK_MOD_NAME, VAR_UAP_SSID, g_uap_ssid,
			sizeof(g_uap_ssid)) == WM_SUCCESS) {
		dbg("Using %s as the uAP SSID", g_uap_ssid);
		appln_cfg.ssid = g_uap_ssid;
		appln_cfg.hostname = g_uap_ssid;
	} else {
			uint8_t my_mac[6];

			memset(g_uap_ssid, 0, sizeof(g_uap_ssid));
			wlan_get_mac_address(my_mac);
			/* Provisioning SSID */
			snprintf(g_uap_ssid, sizeof(g_uap_ssid),
				 "aio-%02X%02X", my_mac[4], my_mac[5]);
			dbg("Using %s as the uAP SSID", g_uap_ssid);
			appln_cfg.ssid = g_uap_ssid;
			appln_cfg.hostname = g_uap_ssid;
	}
}

#define KEY_LEN 16
uint8_t prov_key[KEY_LEN + 1]; /* One extra length to store \0" */

int aio_get_prov_key(uint8_t *prov_key)
{

	if (psm_get_single(NETWORK_MOD_NAME, VAR_PROV_KEY,
			   (char *)prov_key,
			   KEY_LEN + 1) == WM_SUCCESS) {
		if (strlen((char *)prov_key) == KEY_LEN) {
			dbg("Using key from psm %s", prov_key);
			prov_ezconn_set_device_key(prov_key, KEY_LEN);
			return KEY_LEN;
		} else {
			dbg("Found incorrect prov_key. Starting provisioning"
			    " without key");
			dbg("You can set 16byte key using below command and "
			    "reboot the board");
			dbg("psm-set network prov_key <16byte key>");
			memset(prov_key, 0 , KEY_LEN);
			return 0;
		}
	} else {
		dbg("No prov_key found. Starting provisioning without key");
		return 0;
	}
}

/*
 * A simple HTTP Web-Service Handler
 *
 * Returns the string "Hello World" when a GET on http://<IP>/hello
 * is done.
 */

char *hello_world_string = "Hello World\n";
static struct json_str jstr;
static int led_state = 1;

int hello_handler(httpd_request_t *req)
{
	char *content = hello_world_string;
	httpd_send_response(req, HTTP_RES_200,
			    content, strlen(content),
			    HTTP_CONTENT_PLAIN_TEXT_STR);
	return WM_SUCCESS;
}

static int aio_get_ui_link(httpd_request_t *req)
{
	return cloud_get_ui_link(req);
}

void aio_led_on()
{
	led_state = 1;
	led_on(board_led_2());
}

void aio_led_off()
{
	led_state = 0;
	led_off(board_led_2());
}

int aio_get_led_state()
{
	return led_state;
}

static int aio_http_get_led_state(httpd_request_t *req)
{
	char content[30];
	json_str_init(&jstr, content, sizeof(content));
	json_start_object(&jstr);
	json_set_val_int(&jstr, "state", led_state ? 1 : 0);
	json_close_object(&jstr);
	return httpd_send_response(req, HTTP_RES_200, content,
		strlen(content), HTTP_CONTENT_JSON_STR);
}

static int aio_http_set_led_state(httpd_request_t *req)
{
	char led_state_change_req[30];
	int ret;

	ret = httpd_get_data(req, led_state_change_req,
			sizeof(led_state_change_req));
	if (ret < 0) {
		dbg("Failed to get post request data");
		return ret;
	}
	/* Parse the JSON data */
	jsontok_t json_tokens[NUM_TOKENS];
	jobj_t jobj;
	ret = json_init(&jobj, json_tokens, NUM_TOKENS, led_state_change_req,
			req->body_nbytes);
	if (ret != WM_SUCCESS)
		return -WM_FAIL;

	int state;
	if (json_get_val_int(&jobj, "state",
			     &state) != WM_SUCCESS)
		return -1;

	if (state)
		aio_led_on();
	else
		aio_led_off();

	dbg("Led state set to %d", led_state);

	return httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_SUCCESS,
				   strlen(HTTPD_JSON_SUCCESS),
				   HTTP_CONTENT_JSON_STR);
}

struct httpd_wsgi_call aio_http_handlers[] = {
	{"/hello", HTTPD_DEFAULT_HDR_FLAGS, 0,
	hello_handler, NULL, NULL, NULL},
	{"/cloud_ui", HTTPD_DEFAULT_HDR_FLAGS | HTTPD_HDR_ADD_PRAGMA_NO_CACHE,
	0, aio_get_ui_link, NULL, NULL, NULL},
	{"/led-state", HTTPD_DEFAULT_HDR_FLAGS | HTTPD_HDR_ADD_PRAGMA_NO_CACHE,
	0, aio_http_get_led_state, aio_http_set_led_state, NULL, NULL},
};

static int aio_handlers_no =
	sizeof(aio_http_handlers) / sizeof(struct httpd_wsgi_call);

	/* This function is defined for handling critical error.
	 * For this application, we just stall and do nothing when
	 * a critical error occurs.
	 */
void critical_error(int crit_errno, void *data)
{
	dbg("Critical Error %d: %s\r\n", crit_errno,
			critical_error_msg(crit_errno));
	while (1)
		;
	/* do nothing -- stall */
}

/*
 * Register Web-Service handlers
 *
 */
int register_httpd_handlers()
{
	return httpd_register_wsgi_handlers(aio_http_handlers,
		aio_handlers_no);
}

/* This function must initialize the variables required (network name,
 * passphrase, etc.) It should also register all the event handlers that are of
 * interest to the application.
 */
int appln_config_init()
{
	/* Initialize service name for mdns */
	snprintf(appln_cfg.servname, MAX_SRVNAME_LEN, "aio");
	appln_cfg.wps_pb_gpio = board_button_1();
	/* Initialize reset to provisioning push button settings */
	appln_cfg.reset_prov_pb_gpio = board_button_2();
	/* Initialize power management */
	hp_pm_init();
	return 0;
}

/*-----------------------Local declarations----------------------*/
static int provisioned;
static uint8_t mdns_announced;

/* This function stops various services when
 * device gets disconnected or reset to provisioning is done.
 */
static void stop_services()
{
	aio_cloud_stop();
}

/* This function starts various services when
 * device get connected to a network.
 */
static void start_services()
{
	dbg("Start Cloud");
	aio_cloud_start();
}
/*
 * Event: INIT_DONE
 *
 * The application framework is initialized.
 *
 * The data field has information passed by boot2 bootloader
 * when it loaded the application.
 *
 * ?? What happens if app is loaded via jtag
 */
static void event_init_done(void *data)
{
#if APPCONFIG_DEBUG_ENABLE
	struct app_init_state *state;
	state = (struct app_init_state *)data;
#endif /* APPCONFIG_DEBUG_ENABLE */

	dbg("Event: INIT_DONE");
	dbg("Factory reset bit status: %d", state->factory_reset);
	dbg("Booting from backup firmware status: %d", state->backup_fw);
	dbg("Previous reboot cause: %u", state->rst_cause);

	int err = os_timer_create(&uap_down_timer,
				  "uap-down-timer",
				  os_msec_to_ticks(UAP_DOWN_TIMEOUT),
				  &uap_down_timer_cb,
				  NULL,
				  OS_TIMER_ONE_SHOT,
				  OS_TIMER_NO_ACTIVATE);
	if (err != WM_SUCCESS) {
		dbg("Unable to start uap down timer");
	}
}

/*
 * Handler invoked on WLAN_INIT_DONE event.
 *
 * When WLAN is started, the application framework looks to
 * see whether a home network information is configured
 * and stored in PSM (persistent storage module).
 *
 * The data field returns whether a home network is provisioned
 * or not, which is used to determine what network interfaces
 * to start (station, micro-ap, or both).
 *
 * If provisioned, the station interface of the device is
 * connected to the configured network.
 *
 * Else, Micro-AP network is configured.
 *
 * (If desired, the Micro-AP network can also be started
 * along with the station interface.)
 *
 * We also start all the services which don't need to be
 * restarted between provisioned and non-provisioned mode
 * or between connected and disconnected state.
 *
 * Accordingly:
 *      -- Start mDNS and advertize services
 *	-- Start HTTP Server
 *	-- Register WSGI handlers for HTTP server
 */
static void event_wlan_init_done(void *data)
{
	int ret;
#ifdef APPCONFIG_HTTPS_SERVER
	httpd_tls_certs_t httpd_tls_certs = {
		.server_cert = server_cert,
		.server_cert_size = sizeof(server_cert) - 1,
		.server_key = server_key,
		.server_key_size = sizeof(server_key) - 1,
		.client_cert = NULL,
		.client_cert_size = 0,
	};
#endif /* APPCONFIG_HTTPS_SERVER */

	/* We receive provisioning status in data */
	provisioned = (int)data;

	dbg("Event: WLAN_INIT_DONE provisioned=%d", provisioned);

	/* Initialize ssid to be used for uAP mode */
	appln_init_ssid();

	if (provisioned) {
		app_sta_start();
		/* Load  CLOUD overlay in memory */
		aio_load_cloud_overlay();
	} else {
#ifndef APPCONFIG_PROV_EZCONNECT
		app_uap_start_with_dhcp(appln_cfg.ssid, appln_cfg.passphrase);
		/* Load WPS overlay in memory */
		aio_load_wps_overlay();
#else
		dbg("");
		dbg("EZConnect provisioning protocol v2");
		dbg("Please make sure that you are using the phone "
		    "app(iOS/Android)");
		dbg("that displays EZConnect provisioning protocol version 2");
		dbg("");
		int keylen = aio_get_prov_key(prov_key);
		app_ezconnect_provisioning_start(appln_cfg.ssid, prov_key,
						 keylen);
#endif
	}

	if (provisioned)
		hp_configure_reset_prov_pushbutton();

#if APPCONFIG_MDNS_ENABLE
	/*
	 * Start mDNS and advertize our hostname using mDNS
	 */
	dbg("Starting mdns");
	app_mdns_start(appln_cfg.hostname);
#endif /* APPCONFIG_MDNS_ENABLE */


#ifdef APPCONFIG_HTTPS_SERVER
	wmtime_time_set_posix(EPOCH_TIME);
	ret = httpd_use_tls_certificates(&httpd_tls_certs);
#endif /* APPCONFIG_HTTPS_SERVER */
	/*
	 * Start http server and enable webapp in the
	 * FTFS partition on flash
	 */
	ret = app_httpd_with_fs_start(ftfs_api_version, ftfs_part_name, &fs);
	if (ret != WM_SUCCESS)
		dbg("Error: Failed to start HTTPD");

	/*
	 * Register /hello http handler
	 */
	register_httpd_handlers();

	/*
	 * Initialize CLI Commands for some of the modules:
	 *
	 * -- psm:  allows user to check data in psm partitions
	 * -- ftfs: allows user to see contents of ftfs
	 * -- wlan: allows user to explore basic wlan functions
	 */

	ret = psm_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: psm_cli_init failed");
	ret = ftfs_cli_init(fs);
	if (ret != WM_SUCCESS)
		dbg("Error: ftfs_cli_init failed");
	ret = rfget_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: rfget_cli_init failed");
	/* Enable all the wlan CLIs when XIP is enabled. In case of non XIP
	 * due to memory constraints enable only basic wlan CLIs */
#ifdef APPCONFIG_XIP_ENABLE
	ret = wlan_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_cli_init failed");
#else
	ret = wlan_basic_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_basic_cli_init failed");
#endif
	if (!provisioned) {
		/* Start Slow Blink */
		led_slow_blink(board_led_1());
	}
}

/*
 * Event: Micro-AP Started
 *
 * If we are not provisioned, then start provisioning on
 * the Micro-AP network.
 *
 * Also, enable WPS.
 *
 * Since Micro-AP interface is UP, announce mDNS service
 * on the Micro-AP interface.
 */
static void event_uap_started(void *data)
{
#ifndef APPCONFIG_PROV_EZCONNECT
	void *iface_handle = net_get_uap_handle();

	dbg("Event: Micro-AP Started");
	if (!provisioned) {
		dbg("Starting provisioning");
#if APPCONFIG_WPS_ENABLE
		hp_configure_wps_pushbutton();
		aio_wps_cli_init();
		app_provisioning_start(PROVISIONING_WLANNW |
				       PROVISIONING_WPS);
#else
		app_provisioning_start(PROVISIONING_WLANNW);
#endif /* APPCONFIG_WPS_ENABLE */

	}
	hp_mdns_announce(iface_handle, UP);
#endif
}

/*
 * Event: A wireless client is associated with Micro-AP
 */
static void event_uap_client_assoc(void *data)
{
}

/*
 * Event: A wireless client is dissociated from Micro-AP
 */
static void event_uap_client_dissoc(void *data)
{
}

/*
 * Event: PROV_DONE
 *
 * Provisioning is complete. We can stop the provisioning
 * service.
 *
 * Stop WPS.
 *
 * Enable Reset to Prov Button.
 */
static void event_prov_done(void *data)
{
	hp_configure_reset_prov_pushbutton();
#ifndef APPCONFIG_PROV_EZCONNECT
#if APPCONFIG_WPS_ENABLE
	hp_unconfigure_wps_pushbutton();
	aio_wps_cli_deinit();
#endif /* APPCONFIG_WPS_ENABLE */
	app_provisioning_stop();
#else
	app_ezconnect_provisioning_stop();
#endif /* APPCONFIG_PROV_EZCONNECT */
	dbg("Provisioning successful");
}

/* Event: PROV_CLIENT_DONE
 *
 * Provisioning Client has terminated session.
 *
 * We can now safely stop the Micro-AP network.
 *
 * Note: It is possible to keep the Micro-AP network alive even
 * when the provisioning client is done.
 */
static void event_prov_client_done(void *data)
{
#ifndef APPCONFIG_PROV_EZCONNECT
	int ret;

	hp_mdns_deannounce(net_get_uap_handle());
	ret = app_uap_stop();
	if (ret != WM_SUCCESS)
		dbg("Error: Failed to Stop Micro-AP");
#endif
}

/*
 * Event UAP_STOPPED
 *
 * Normally, we will get here when provisioning is complete,
 * and the Micro-AP network is brought down.
 *
 * If we are connected to an AP, we can enable IEEE Power Save
 * mode here.
 */
static void event_uap_stopped(void *data)
{
	dbg("Event: Micro-AP Stopped");
	hp_pm_wifi_ps_enable();
}

/*
 * Event: PROV_WPS_SSID_SELECT_REQ
 *
 * An SSID with active WPS session is found and WPS negotiation will
 * be started with this AP.
 *
 * Since WPS take a lot of memory resources (on the heap), we
 * temporarily stop http server (and, the Micro-AP provisioning
 * along with it).
 *
 * The HTTP server will be restarted when WPS session is over.
 */
static void event_prov_wps_ssid_select_req(void *data)
{
	int ret;

	ret = app_httpd_stop();
	if (ret != WM_SUCCESS) {
		dbg("Error stopping HTTP server");
	}
}

/*
 * Event: PROV_WPS_SUCCESSFUL
 *
 * WPS session completed successfully.
 *
 * Restart the HTTP server that was stopped when WPS session attempt
 * began.
 */
static void event_prov_wps_successful(void *data)
{
	int ret;

	ret = app_httpd_with_fs_start(ftfs_api_version, ftfs_part_name, &fs);
	if (ret != WM_SUCCESS) {
		dbg("Error starting HTTP server");
	}

	return;
}

/*
 * Event: PROV_WPS_UNSUCCESSFUL
 *
 * WPS session completed unsuccessfully.
 *
 * Restart the HTTP server that was stopped when WPS session attempt
 * began.
 */
static void event_prov_wps_unsuccessful(void *data)
{
	int ret;

	ret = app_httpd_with_fs_start(ftfs_api_version, ftfs_part_name, &fs);
	if (ret != WM_SUCCESS) {
		dbg("Error starting HTTP server");
	}
	return;
}

/*
 * Event: CONNECTING
 *
 * We are attempting to connect to the Home Network
 *
 * Note: We can come here:
 *
 *   1. After boot -- if already provisioned.
 *   2. After provisioning
 *   3. After link loss
 *
 * This is just a transient state as we will either get
 * CONNECTED or have a CONNECTION/AUTH Failure.
 *
 */
static void event_normal_connecting(void *data)
{
	net_dhcp_hostname_set(appln_cfg.hostname);
	dbg("Connecting to Home Network");
	/* Start Fast Blink */
	led_fast_blink(board_led_1());
}

/* Event: AF_EVT_NORMAL_CONNECTED
 *
 * Station interface connected to home AP.
 *
 * Network dependent services can be started here. Note that these
 * services need to be stopped on disconnection and
 * reset-to-provisioning event.
 */
static void event_normal_connected(void *data)
{
	void *iface_handle;
	char ip[16];

	led_on(board_led_1());

	app_network_ip_get(ip);
	dbg("Connected to Home Network with IP address = %s", ip);

	iface_handle = net_get_sta_handle();
	if (!mdns_announced) {
		hp_mdns_announce(iface_handle, UP);
		mdns_announced = 1;
	} else {
		hp_mdns_down_up(iface_handle);
	}
	/* Load CLOUD overlay in memory */
	aio_load_cloud_overlay();
	start_services();
	/*
	 * If micro AP interface is up
	 * queue a timer which will take
	 * micro AP interface down.
	 */
	if (is_uap_started()) {
		os_timer_activate(&uap_down_timer);
		return;
	}

	hp_pm_wifi_ps_enable();
}

/*
 * Event: CONNECT_FAILED
 *
 * We attempted to connect to the Home AP, but the connection did
 * not succeed.
 *
 * This typically indicates:
 *
 * -- Authentication failed.
 * -- The access point could not be found.
 * -- We did not get a valid IP address from the AP
 *
 */
static void event_connect_failed(void *data)
{
	char failure_reason[32];

	if (*(app_conn_failure_reason_t *)data == AUTH_FAILED)
		strcpy(failure_reason, "Authentication failure");
	if (*(app_conn_failure_reason_t *)data == NETWORK_NOT_FOUND)
		strcpy(failure_reason, "Network not found");
	if (*(app_conn_failure_reason_t *)data == DHCP_FAILED)
		strcpy(failure_reason, "DHCP failure");

	os_thread_sleep(os_msec_to_ticks(2000));
	dbg("Application Error: Connection Failed: %s", failure_reason);
	led_off(board_led_1());
}

/*
 * Event: USER_DISCONNECT
 *
 * This means that the application has explicitly requested a network
 * disconnect
 *
 */
static void event_normal_user_disconnect(void *data)
{
	led_off(board_led_1());
	dbg("User disconnect");
}

/*
 * Event: LINK LOSS
 *
 * We lost connection to the AP.
 *
 * The App Framework will attempt to reconnect. We dont
 * need to do anything here.
 */
static void event_normal_link_lost(void *data)
{
	dbg("Link Lost");
}

static void event_normal_pre_reset_prov(void *data)
{
	hp_mdns_deannounce(net_get_mlan_handle());
}

static void event_normal_dhcp_renew(void *data)
{
	void *iface_handle = net_get_mlan_handle();
	hp_mdns_announce(iface_handle, REANNOUNCE);
}

static void event_normal_reset_prov(void *data)
{
	led_on(board_led_1());
	/* Start Slow Blink */
	led_slow_blink(board_led_1());

	/* Stop services like cloud */
	stop_services();

	/* Cancel the UAP down timer timer */
	os_timer_deactivate(&uap_down_timer);

	hp_pm_wifi_ps_disable();
	/* Load WPS overlay in memory */
	aio_load_wps_overlay();

	/* Reset to provisioning */
	provisioned = 0;
	mdns_announced = 0;
	hp_unconfigure_reset_prov_pushbutton();
#ifndef APPCONFIG_PROV_EZCONNECT
	if (is_uap_started() == false) {
		app_uap_start_with_dhcp(appln_cfg.ssid, appln_cfg.passphrase);
	} else {
#ifdef APPCONFIG_WPS_ENABLE
		hp_configure_wps_pushbutton();
		aio_wps_cli_init();
		app_provisioning_start(PROVISIONING_WLANNW |
				       PROVISIONING_WPS);
#else
		app_provisioning_start(PROVISIONING_WLANNW);
#endif /* APPCONFIG_WPS_ENABLE */
	}
#else
	int keylen = aio_get_prov_key(prov_key);
	app_ezconnect_provisioning_start(appln_cfg.ssid, prov_key, keylen);
#endif
}
void ps_state_to_desc(char *ps_state_desc, int ps_state)
{
	switch (ps_state) {
	case WLAN_IEEE:
		strcpy(ps_state_desc, "IEEE PS");
		break;
	case WLAN_DEEP_SLEEP:
		strcpy(ps_state_desc, "Deep sleep");
		break;
	case WLAN_PDN:
		strcpy(ps_state_desc, "Power down");
		break;
	case WLAN_ACTIVE:
		strcpy(ps_state_desc, "WLAN Active");
		break;
	default:
		strcpy(ps_state_desc, "Unknown");
		break;
	}
}

/*
 * Event: PS ENTER
 *
 * Application framework event to indicate the user that WIFI
 * has entered Power save mode.
 */

static void event_ps_enter(void *data)
{
	int ps_state = (int) data;
	char ps_state_desc[32];
	if (ps_state == WLAN_PDN) {
		dbg("NOTE: Due to un-availability of "
		    "enough dynamic memory for ");
		dbg("de-compression of WLAN Firmware, "
		    "exit from PDn will not\r\nwork with aio.");
		dbg("Instead of aio, pm_mc200_wifi_demo"
		    " application demonstrates\r\nthe seamless"
		    " exit from PDn functionality today.");
		dbg("This will be fixed in subsequent "
		    "software release.\r\n");
	}
	ps_state_to_desc(ps_state_desc, ps_state);
	dbg("Power save enter : %s", ps_state_desc);

}

/*
 * Event: PS EXIT
 *
 * Application framework event to indicate the user that WIFI
 * has exited Power save mode.
 */

static void event_ps_exit(void *data)
{
	int ps_state = (int) data;
	char ps_state_desc[32];
	ps_state_to_desc(ps_state_desc, ps_state);
	dbg("Power save exit : %s", ps_state_desc);
}

/* This is the main event handler for this project. The application framework
 * calls this function in response to the various events in the system.
 */
int common_event_handler(int event, void *data)
{
	switch (event) {
	case AF_EVT_INIT_DONE:
		event_init_done(data);
		break;
	case AF_EVT_WLAN_INIT_DONE:
		event_wlan_init_done(data);
		break;
	case AF_EVT_NORMAL_CONNECTING:
		event_normal_connecting(data);
		break;
	case AF_EVT_NORMAL_CONNECTED:
		event_normal_connected(data);
		break;
	case AF_EVT_NORMAL_CONNECT_FAILED:
		event_connect_failed(data);
		break;
	case AF_EVT_NORMAL_LINK_LOST:
		event_normal_link_lost(data);
		break;
	case AF_EVT_NORMAL_USER_DISCONNECT:
		event_normal_user_disconnect(data);
		break;
	case AF_EVT_NORMAL_DHCP_RENEW:
		event_normal_dhcp_renew(data);
		break;
	case AF_EVT_PROV_WPS_SSID_SELECT_REQ:
		event_prov_wps_ssid_select_req(data);
		break;
	case AF_EVT_PROV_WPS_SUCCESSFUL:
		event_prov_wps_successful(data);
		break;
	case AF_EVT_PROV_WPS_UNSUCCESSFUL:
		event_prov_wps_unsuccessful(data);
		break;
	case AF_EVT_NORMAL_PRE_RESET_PROV:
		event_normal_pre_reset_prov(data);
		break;
	case AF_EVT_NORMAL_RESET_PROV:
		event_normal_reset_prov(data);
		break;
	case AF_EVT_UAP_STARTED:
		event_uap_started(data);
		break;
	case AF_EVT_UAP_CLIENT_ASSOC:
		event_uap_client_assoc(data);
		break;
	case AF_EVT_UAP_CLIENT_DISSOC:
		event_uap_client_dissoc(data);
		break;
	case AF_EVT_UAP_STOPPED:
		event_uap_stopped(data);
		break;
	case AF_EVT_PROV_DONE:
		event_prov_done(data);
		break;
	case AF_EVT_PROV_CLIENT_DONE:
		event_prov_client_done(data);
		break;
	case AF_EVT_PS_ENTER:
		event_ps_enter(data);
		break;
	case AF_EVT_PS_EXIT:
		event_ps_exit(data);
		break;
	default:
		break;
	}

	return 0;
}

static void modules_init()
{
	int ret;

	ret = wmstdio_init(UART0_ID, 0);
	if (ret != WM_SUCCESS) {
		dbg("Error: wmstdio_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	ret = cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: cli_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	/* Initialize time subsystem.
	 *
	 * Initializes time to 1/1/1970 epoch 0.
	 */
	ret = wmtime_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: wmtime_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	/*
	 * Register Time CLI Commands
	 */
	ret = wmtime_cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: wmtime_cli_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	/*
	 * Initialize Power Management Subsystem
	 */
	ret = pm_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: pm_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	/*
	 * Register Power Management CLI Commands
	 */
	ret = pm_cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: pm_cli_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	ret = pm_mcu_cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: pm_mc200_cli_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	ret = gpio_drv_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: gpio_drv_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	ret = healthmon_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: healthmon_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	ret = healthmon_start();
	if (ret != WM_SUCCESS) {
		dbg("Error: healthmon_start failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}
/* Set the final_about_to_die handler of the healthmon */
	healthmon_set_final_about_to_die_handler
		((void (*)())diagnostics_write_stats);

	app_sys_register_diag_handler();
	app_sys_register_upgrade_handler();

#if defined(APPCONFIG_XIP_ENABLE) && !defined(CONFIG_ENABLE_MCU_PM3)
	ret = sysinfo_init();
	if (ret != WM_SUCCESS)
		dbg("Error: Sysinfo cli init failed");
	boot_report_flags();
#endif

	/* Turn on the LED */
	aio_led_on();

	return;
}

int main()
{
	modules_init();

	dbg("Build Time: " __DATE__ " " __TIME__ "");

	appln_config_init();

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		dbg("Failed to start application framework");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	return 0;
}
