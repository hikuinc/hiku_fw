/*
 *  Copyright (C) 2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* WLAN Application
 *
 * This application should serve as a good starting point for application
 * development.
 *
 * It demonstrates the basic functionalities required to provision and
 * communicate with the device.
 *
 * The typical scenario in which you would use wm_demo, is to copy it as your
 * project and make modifications to the same.
 *
 * Typically you would have to make following additions,
 * - Create any HTTP WSGI handlers if you wish to do local network control.
 *   These can be registered in event_wlan_init().
 *   (refer to module_demo/webhandler_demo in sample_apps)
 * - Start cloud interfacing servicing in event_normal_connected().
 *   (refer to cloud_demo in sample_apps)
 * - Write any device drivers for communicating with your peripherals
 *   (refer to io_demo/ in sample_apps)
 * - Manage firmware upgrades
 *   (refer to module_demo/fw_upgrade_demo in sample_apps)
 * - Fix your LED indicators in file led-indicators.
 *   (make changes in ./led_indications.c)
 *
 * This application showcases key features of the SDK, from provisioning the
 * device for the first time, to advertising mdns services, to providing HTTP
 * interfaces for interacting with the device.
 *
 * This application supports a unique provisioning method EZConnect provisioning
 * which configures the device without requiring user to switch his smart phone
 * network using sniffer based provisioning. In case, for some reason, this
 * method fails, user can revert back to micro-AP based provisioning without
 * requiring any action on the device.
 *
 * Description:
 *
 * The application is written using Application Framework that simplifies
 * development of WLAN networking applications.
 *
 * WLAN Initialization:
 *
 * When the application framework is started, it starts up the WLAN
 * sub-system and initializes the network stack. The app receives the event
 * when the WLAN subsystem has been started and initialized.
 *
 * If the device is not provisioned, application framework sends an event
 * to the app to start EZConnect provisioning. In this state, device starts
 * sniffer functionality and also starts sending micro-AP beacons. If it
 * successfully receives the network configuration information over sniffer, it
 * goes into provisioned mode. In case, it receives connection request for the
 * micro-AP that it is beaconing, it stops sniffer and allows a full-fledged
 * micro-AP connection to the client. Client then can use secure provisioning
 * web API to provision the device. If device connects and disconnects without
 * provisioning the device, device again enters in sniffer mode after 1 minute.
 *
 * When the device is initialized and ready, app will receive a PROV_STARTED
 * event.
 *
 * PROV_STARTED:
 *
 * The device is now up and ready to receive network configuration settings.
 *
 * In case of EZConnect provisioning, user can send configuration settings using
 * a EZConnect mobile app.
 *
 * In case of micro-AP based provisioning the app starts the web-server which
 * will then listen to incoming http connections (e.g. from a browser).
 *
 * By default, device can be accessed at URI http://192.168.10.1
 * Through the Web UI, user can provision the device to an Access Point.
 * Once the device is provisioned successfully to the configured Access
 * Point, app will receive a CONNECTED event.
 *
 * CONNECTED:
 *
 * Now the station interface of the device is up and app takes the
 * EZConnect provisioning interface down.
 *
 * Default LED indications for the states are defined in led_indications.c and
 * can be modified as per the requirement.
 *
 *
 */

#include <app_framework.h>
#include <psm.h>
#include <ftfs.h>
#include <reset_prov_helper.h>
#include <mdns_helper.h>
#include <critical_error.h>

#include "utility.h"
#include "led_indications.h"

static uint8_t mdns_announced;
appln_config_t appln_cfg;
static char    uap_ssid[IEEEtypes_SSID_SIZE + 1];
struct fs      *fs_handle;

/* Initialize the device configurations like network name, provisioning key
 * etc. with the required values. App developer can change the default
 * values with the app specific values.
 */
int appln_config_init()
{
	int ret;

	/* Service name for mdns */
#define MDNS_SERVICE_NAME "wm_demo"
	snprintf(appln_cfg.servname, MAX_SRVNAME_LEN, MDNS_SERVICE_NAME);

	/* Name of the uAP SSID */
#define UAP_PREFIX "wmdemo"
	ret = get_prov_ssid(UAP_PREFIX, sizeof(UAP_PREFIX),
			    uap_ssid, sizeof(uap_ssid));
	if (ret != WM_SUCCESS)
		return ret;

	appln_cfg.ssid = uap_ssid;

	/* Hostname */
	appln_cfg.hostname = appln_cfg.ssid;

	/* Provisioning key */
#define PROV_KEY    "device_key"
	/* App developer can change the default provisioning key.
	 * Ideally, a unique provisioning key will be used for each device.
	 * This should be programmed into the device at the time of
	 * manufacturing and should be read from the appropriate flash partition
	 * and setup in here.
	 */
	ret = set_prov_key((uint8_t *)PROV_KEY, strlen(PROV_KEY));
	if (ret != WM_SUCCESS)
		return ret;

	/* Reset to Factory/Provisioning */
	appln_cfg.reset_prov_pb_gpio = board_button_2();
	return WM_SUCCESS;
}

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

/* Set the final action handler to be called by the healthmon before device
 * resets due to the watchdog. Typically this can be used to log diagnostics
 * information captured by all the other about_to_die handlers, or any other
 * application specific functions.
 */
void final_about_to_die()
{
	/* do nothing ... */
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
 * Else, provisioning framework is configured.
 *
 * (If desired, the Micro-AP network can also be started
 * along with the station interface.)
 *
 * We also start all the services which don't need to be
 * restarted between provisioned and non-provisioned mode
 * or between connected and disconnected state.
 *
 * Accordingly:
 *	-- Start HTTP Server
 */
static void event_wlan_init_done(void *data)
{
	int ret, key_len;
	uint8_t key[EZCONN_MAX_KEY_LEN + 1] = {0};

	appln_config_init();

	/* We receive provisioning status in data */
	int provisioned = (int)data;

	dbg("Event: WLAN_INIT_DONE provisioned=%d", provisioned);

	if (provisioned) {
		dbg("Starting station");
		app_sta_start();
	} else {
		key_len = get_prov_key(key, sizeof(key) - 1);
		if (key_len < WM_SUCCESS) {
			dbg("Error: Failed to get provisioning key");
			return;
		}

		dbg("Launching provisioning with ssid %s with key %s (%d)",
		    appln_cfg.ssid, key, key_len);
		ret = app_ezconnect_provisioning_start(appln_cfg.ssid, key,
						       key_len);
		if (ret != WM_SUCCESS) {
			dbg("Error: Failed to start EZConnect provisioning");
			return;
		}
	}

	if (provisioned)
		hp_configure_reset_prov_pushbutton();

	/*
	 * Start http server
	 */
	ret = app_httpd_only_start();
	if (ret != WM_SUCCESS)
		dbg("Error: Failed to start HTTPD");

	/*
	 * Enable webapp in the FTFS partition on flash
	 */
#define FTFS_API_VERSION    100
#define FTFS_PART_NAME      "ftfs"
	app_ftfs_init(FTFS_API_VERSION, FTFS_PART_NAME, &fs_handle);

	/*
	 * Start mDNS and advertize our hostname using mDNS
	 */
	app_mdns_start(appln_cfg.hostname);

	/*
	 * Initialize CLI Commands for some of the modules:
	 *
	 * -- psm:  allows user to check data in psm partitions
	 * -- ftfs: allows user to see contents of ftfs
	 * -- wlan: allows user to explore basic wlan functions
	 */

#ifdef APPCONFIG_DEBUG_ENABLE
	ret = psm_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: psm_cli_init failed");
	ret = ftfs_cli_init(fs_handle);
	if (ret != WM_SUCCESS)
		dbg("Error: ftfs_cli_init failed");
#endif /* APPCONFIG_DEBUG_ENABLE */

#ifdef APPCONFIG_DEBUG_ENABLE
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
#endif /* APPCONFIG_XIP_ENABLE */
#endif /* APPCONFIG_DEBUG_ENABLE */

	if (!provisioned) {
		indicate_provisioning_state();
	}
}

/*
 * Event: PROV_DONE
 *
 * Provisioning is complete. We can stop the provisioning
 * service.
 */
static void event_prov_done(void *data)
{
	dbg("Provisioning Completed");
	app_ezconnect_provisioning_stop();
	hp_configure_reset_prov_pushbutton();
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
	indicate_normal_connecting_state();
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

	app_network_ip_get(ip);
	dbg("Connected to Home Network with IP address = %s", ip);

	iface_handle = net_get_sta_handle();
	if (!mdns_announced) {
		hp_mdns_announce(iface_handle, UP);
		mdns_announced = 1;
	} else {
		hp_mdns_down_up(iface_handle);
	}
	indicate_normal_connected_state();
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

	dbg("Application Error: Connection Failed: %s", failure_reason);
}

/*
 * Event: DHCP_RENEW
 *
 * DHCP Lease has been renewed. Reannounce our mDNS service, in case the IP
 * Address changed.
 */
static void event_normal_dhcp_renew(void *data)
{
	void *iface_handle = net_get_mlan_handle();
	hp_mdns_announce(iface_handle, REANNOUNCE);
}

/*
 * Event: PRE_RESET_PROV
 *
 * We are just about to reset to the provisioning network.
 *
 * If any mDNS services have been announced, deannounce them.
 */
static void event_normal_pre_reset_prov(void *data)
{
	hp_mdns_deannounce(net_get_mlan_handle());
}

/*
 * Event: Reset to Provisioning
 *
 * Start the provisioning framework back again.
 *
 */
static void event_normal_reset_prov(void *data)
{
	uint8_t key[EZCONN_MAX_KEY_LEN + 1] = {0};
	int key_len;
	mdns_announced = 0;
	hp_unconfigure_reset_prov_pushbutton();
	key_len = get_prov_key(key, sizeof(key) - 1);
	if (key_len < WM_SUCCESS) {
		dbg("Error: Failed to get provisioning key");
		return;
	}
	dbg("Launching provisioning with ssid %s with key %s (%d)",
	    appln_cfg.ssid, key, key_len);
	if (app_ezconnect_provisioning_start(appln_cfg.ssid, key,
					     key_len) != WM_SUCCESS)
		dbg("Error: Failed to start EZConnect provisioning");
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
	case AF_EVT_PROV_DONE:
		event_prov_done(data);
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
	case AF_EVT_NORMAL_DHCP_RENEW:
		event_normal_dhcp_renew(data);
		break;
	case AF_EVT_NORMAL_PRE_RESET_PROV:
		event_normal_pre_reset_prov(data);
		break;
	case AF_EVT_NORMAL_RESET_PROV:
		event_normal_reset_prov(data);
		break;
	default:
		break;
	}

	return WM_SUCCESS;
}

int main()
{
	modules_init();

	dbg("Build Time: " __DATE__ " " __TIME__ "");

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		dbg("Failed to start application framework");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	return WM_SUCCESS;
}
