/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 *
 * Application for testing uAP and station connection phase in continuous loop.
 *
 * Summary:
 *
 * Application is created to verify uAP and station functionality in
 * continuous loop for long duration.
 *
 */

#include <wm_os.h>
#include <app_framework.h>
#include <wmtime.h>
#include <partition.h>
#include <appln_cb.h>
#include <appln_dbg.h>
#include <cli.h>
#include <psm.h>
#include <wmstdio.h>
#include <wmsysinfo.h>
#include <wm_net.h>
#include <mdns_helper.h>
#include <wps_helper.h>
#include <reset_prov_helper.h>
#include <httpd.h>
#include <ftfs.h>
#include <led_indicator.h>
#include <board.h>
#include <dhcp-server.h>
#include <mdev_gpio.h>
#include <critical_error.h>

/*-----------------------Global declarations----------------------*/

appln_config_t appln_cfg = {
	.ssid = "Marvell Prov Demo",
	.passphrase = "marvellwm",
	.hostname = "provdemo",
};

int ftfs_api_version = 100;
char *ftfs_part_name = "ftfs";

static uint8_t first_time = 1;
struct fs *fs;
struct wlan_network sta_net;
static int sta_count, uap_count;

/*-----------------------Global declarations----------------------*/
static int provisioned;
static uint8_t mdns_announced;

/* This function must initialize the variables required (network name,
 * passphrase, etc.) It should also register all the event handlers that are of
 * interest to the application.
 */
int appln_config_init()
{
	/* Initialize service name for mdns */
	snprintf(appln_cfg.servname, MAX_SRVNAME_LEN, "provdemo");
	appln_cfg.wps_pb_gpio = board_button_1();
	/* Initialize reset to provisioning push button settings */
	appln_cfg.reset_prov_pb_gpio = board_button_2();
	return 0;
}

/* This function is defined for handling critical error.
 * For this application, we just stall and do nothing when
 * a critical error occurs.
 */
void critical_error(int crit_errno, void *data)
{
	wmprintf("Critical Error %d: %s\r\n", crit_errno,
			critical_error_msg(crit_errno));
	while (1)
		;
	/* do nothing -- stall */
}

static void cmd_app_name(int argc, char *argv[])
{
	wmprintf("qa3\r\n");
}


static struct cli_command commands[] = {
	{"app-name", NULL, cmd_app_name},
};

int app_name_cli_init(void)
{
	int i;

	for (i = 0; i < sizeof(commands) / sizeof(struct cli_command); i++)
		if (cli_register_command(&commands[i]))
			return 1;
	return 0;
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
 *	-- Start mDNS and advertize services
 *	-- Start HTTP Server
 *	-- Register WSGI handlers for HTTP server
 */
void event_wlan_init_done(void *data)
{
	int ret;
	/* We receive provisioning status in data */
	provisioned = (int)data;

	bzero(&sta_net, sizeof(sta_net));
	/* Set SSID as passed by the user */
	strncpy(sta_net.ssid, "QA3", sizeof(sta_net.ssid));
	sta_net.security.type = WLAN_SECURITY_WPA2;
	/* Set the passphrase */
	strncpy(sta_net.security.psk, "1234567890",
			sizeof(sta_net.security.psk));
	sta_net.security.psk_len = strlen(sta_net.security.psk);
	/* Set profile name */
	strcpy(sta_net.name, "sta-network");
	/* Set channel selection to auto (0) */
	sta_net.channel = 0;
	/* Set network type to STA */
	sta_net.type = WLAN_BSS_TYPE_STA;
	/* Set network role to STA */
	sta_net.role = WLAN_BSS_ROLE_STA;
	/* Specify address type as dynamic assignment */
	sta_net.ip.ipv4.addr_type = ADDR_TYPE_DHCP;

	dbg("AF_EVT_WLAN_INIT_DONE provisioned=%d", provisioned);

	if (provisioned) {
		app_sta_start();
	} else {
		app_uap_start_with_dhcp(appln_cfg.ssid, appln_cfg.passphrase);
	}

	if (provisioned)
		hp_configure_reset_prov_pushbutton();

	/*
	 * Start mDNS and advertize our hostname using mDNS
	 */
	dbg("Starting mdns");
	app_mdns_start(appln_cfg.hostname);

	/*
	 * Start http server and enable webapp in the
	 * FTFS partition on flash
	 */
	ret = app_httpd_with_fs_start(ftfs_api_version, ftfs_part_name, &fs);
	if (ret != WM_SUCCESS)
		dbg("Failed to start HTTPD");

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
	ret = wlan_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_cli_init failed");
	ret = app_name_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: app_name_cli_init failed");

	if (!provisioned) {
		/* Start Slow Blink */
		led_slow_blink(board_led_2());
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
void event_uap_started(void *data)
{
	void *iface_handle = net_get_uap_handle();

	if (!provisioned) {
		dbg("Starting provisioning");
		hp_configure_wps_pushbutton();
		app_provisioning_start(PROVISIONING_WLANNW | PROVISIONING_WPS);
	}
	dbg("mdns uap up event");
	hp_mdns_announce(iface_handle, UP);
	uap_count++;
	wmprintf("%d => uAP started\r\n", uap_count);
	os_thread_sleep(os_msec_to_ticks(2000));
	hp_unconfigure_wps_pushbutton();
	app_provisioning_stop();
	app_uap_stop();
	dhcp_server_stop();
}

void event_normal_reset_prov(void *data)
{
	led_on(board_led_2());
	first_time = 1;

	/* Start Slow Blink */
	led_slow_blink(board_led_2());

	/* Reset to provisioning */
	provisioned = 0;
	mdns_announced = 0;
	hp_unconfigure_reset_prov_pushbutton();
	if (is_uap_started() == false) {
		app_uap_start_with_dhcp(appln_cfg.ssid, appln_cfg.passphrase);
	} else {
		hp_configure_wps_pushbutton();
		app_provisioning_start(PROVISIONING_WLANNW | PROVISIONING_WPS);
	}
}

void event_prov_done(void *data)
{
	hp_configure_reset_prov_pushbutton();
	hp_unconfigure_wps_pushbutton();
	app_provisioning_stop();
	dbg("Provisioning successful");
}

void event_prov_client_done(void *data)
{
	app_uap_stop();
	dhcp_server_stop();
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

	wmprintf("%d => uAP stopped\r\n", uap_count);
	app_sta_start_by_network(&sta_net);
	os_thread_sleep(os_msec_to_ticks(2000));
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
void event_prov_wps_ssid_select_req(void *data)
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
void event_prov_wps_successful(void *data)
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
void event_prov_wps_unsuccessful(void *data)
{
	int ret;

	ret = app_httpd_with_fs_start(ftfs_api_version, ftfs_part_name, &fs);
	if (ret != WM_SUCCESS) {
		dbg("Error starting HTTP server");
	}
	return;
}

void event_normal_connecting(void *data)
{
	net_dhcp_hostname_set(appln_cfg.hostname);
	dbg("Connecting to Home Network");
	/* Start Fast Blink */
	led_fast_blink(board_led_2());
}

/* Event: AF_EVT_NORMAL_CONNECTED
 *
 * Station interface connected to home AP.
 *
 * Network dependent services can be started here. Note that these
 * services need to be stopped on disconnection and
 * reset-to-provisioning event.
 */
void event_normal_connected(void *data)
{
	void *iface_handle;
	char ip[16];

	led_off(board_led_2());

	app_network_ip_get(ip);
	dbg("Connected to provisioned network with ip address =%s", ip);

	iface_handle = net_get_mlan_handle();
	if (!mdns_announced) {
		hp_mdns_announce(iface_handle, UP);
		mdns_announced = 1;
	} else {
		hp_mdns_down_up(iface_handle);
	}

	sta_count++;
	wmprintf("%d => STA connected\r\n", sta_count);

	app_sta_stop();
	os_thread_sleep(os_msec_to_ticks(2000));
}

/* Event handler for AF_EVT_NORMAL_DISCONNECTED - Station interface
 * disconnected.
 * Stop the network services which need not be running in disconnected mode.
 */
void event_normal_user_disconnect(void *data)
{
	led_on(board_led_2());
	wmprintf("%d => STA disconnected\r\n", sta_count);
	/* restart the micro-ap interface */
	app_uap_start_with_dhcp(appln_cfg.ssid, appln_cfg.passphrase);
	os_thread_sleep(os_msec_to_ticks(2000));
}

void event_normal_link_lost(void *data)
{
	/* is this automatically retried ?? */
	event_normal_user_disconnect(data);
}

void event_normal_dhcp_renew(void *data)
{
	void *iface_handle = net_get_mlan_handle();
	hp_mdns_announce(iface_handle, REANNOUNCE);
}

/* This is the main event handler for this project. The application framework
 * calls this function in response to the various events in the system.
 */
int common_event_handler(int event, void *data)
{
	switch (event) {
	case AF_EVT_WLAN_INIT_DONE:
		event_wlan_init_done(data);
		break;
	case AF_EVT_NORMAL_CONNECTING:
		event_normal_connecting(data);
		break;
	case AF_EVT_NORMAL_CONNECTED:
		event_normal_connected(data);
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
	case AF_EVT_PROV_DONE:
		event_prov_done(data);
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
	case AF_EVT_NORMAL_RESET_PROV:
		event_normal_reset_prov(data);
		break;
	case AF_EVT_UAP_STARTED:
		event_uap_started(data);
		break;
	case AF_EVT_PROV_CLIENT_DONE:
		event_prov_client_done(data);
		break;
	case AF_EVT_UAP_STOPPED:
		event_uap_stopped(data);
	default:
		break;
	}

	return 0;
}

static void modules_init()
{
	int ret;

	/*
	 * Initialize wmstdio prints
	 */
	ret = wmstdio_init(UART0_ID, 0);
	if (ret != WM_SUCCESS) {
		dbg("Error: wmstdio_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	/*
	 * Initialize CLI Commands
	 */
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
	 * Register Power Management CLI Commands
	 */
	ret = pm_cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: pm_cli_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	ret = gpio_drv_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: gpio_drv_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

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
