/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * WLAN High Level Module
 *
 * Summary:
 *
 * Description:
 *
 * WLAN Initialization:
 *
 * TLS Transaction :
 *
 */
#include <wmstdio.h>
#include <app_framework.h>
#include <wmsdk.h>
#include <psm.h>
#include <dhcp-server.h>
#include <aws_utils.h>
#include <led_indicator.h>
#include <ftfs.h>

/* #define ENABLE_DEBUG */
#ifdef ENABLE_DEBUG
#define dbg(_fmt_, ...)				\
	wmprintf("[wlan] "_fmt_"\n\r", ##__VA_ARGS__)
#else
#define dbg(...)
#endif

struct wlan_network sta_net;

struct wlan_highlevel_network {
	char ssid[IEEEtypes_SSID_SIZE + 1];
	char passphrase[WLAN_PSK_MAX_LENGTH];
};

#define DEF_NW_SECURITY      WLAN_SECURITY_WILDCARD

static struct wlan_highlevel_network uap_nw;
static struct wlan_highlevel_network sta_nw;

int ftfs_api_version = 100;
char *ftfs_part_name = "ftfs";
struct fs *fs;

/**** Additional top-level utility functions ****/
static struct custom_http_hdlrs {
	struct httpd_wsgi_call *hdlrs;
	int no;
} custom_hdlrs;

void wm_wlan_set_httpd_handlers(void *hdlrs, int no)
{
	custom_hdlrs.hdlrs = hdlrs;
	custom_hdlrs.no = no;
}

static int add_custom_httpd_handlers()
{
	if (custom_hdlrs.hdlrs) {
		return httpd_register_wsgi_handlers(custom_hdlrs.hdlrs,
						    custom_hdlrs.no);
	}
	return WM_SUCCESS;
}

int invoke_reset_to_factory()
{
	psm_erase_and_init();
	pm_reboot_soc();
	return WM_SUCCESS;
}

/**** AF Callback Handlers ****/
/*
 * Event: INIT_DONE
 *
 * The application framework is initialized.
 *
 * The data field has information passed by boot2 bootloader
 * when it loads the application.
 *
 */
static void event_init_done(void *data)
{
	dbg("Event: INIT_DONE");
}

/*
 * Handler invoked on WLAN_INIT_DONE event.
 *
 */
static int provisioned;
static void event_wlan_init_done(void *data)
{
	int ret;
	provisioned = (int)data;
	char g_uap_ssid[IEEEtypes_SSID_SIZE + 1];
	uint8_t my_mac[6];

	if (strlen(sta_nw.ssid) != 0) {
		bzero(&sta_net, sizeof(sta_net));
		/* Set SSID as passed by the user */
		strncpy(sta_net.ssid, sta_nw.ssid, sizeof(sta_net.ssid));
		if (strlen(sta_nw.passphrase) != 0) {
			sta_net.security.type = WLAN_SECURITY_WPA2;
			/* Set the passphrase */
			strncpy(sta_net.security.psk, sta_nw.passphrase,
				sizeof(sta_net.security.psk));
			sta_net.security.psk_len = strlen(sta_net.security.psk);
		} else {
			sta_net.security.type = WLAN_SECURITY_NONE;
		}
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
		app_sta_start_by_network(&sta_net);
		return;
	}

	wlan_get_mac_address(my_mac);
	if (provisioned) {
		app_sta_start();
		led_fast_blink(board_led_2());
	} else {
		/* to append mac */
		snprintf(g_uap_ssid, sizeof(g_uap_ssid),
			 "%s-%02X%02X", uap_nw.ssid, my_mac[4], my_mac[5]);
		memset(uap_nw.ssid, 0, sizeof(uap_nw.ssid));
		strcpy(uap_nw.ssid, g_uap_ssid);
		/* \to append mac */
		if (strlen(uap_nw.passphrase))
			app_uap_start_with_dhcp(uap_nw.ssid, uap_nw.passphrase);
		else
			app_uap_start_with_dhcp(uap_nw.ssid, NULL);
		wmprintf("Starting micro-AP %s\r\n", uap_nw.ssid);
		/* Start http server and enable webapp in the */
		/* FTFS partition on flash */
		ret = app_httpd_with_fs_start(ftfs_api_version,
					      ftfs_part_name, &fs);
		if (ret != WM_SUCCESS)
			dbg("Failed to start HTTPD");

		app_provisioning_start(PROVISIONING_WLANNW | PROVISIONING_WPS);
		add_custom_httpd_handlers();
		/* Start Slow Blink */
		led_slow_blink(board_led_2());
	}
}


/*
 * Event: CONNECTING
 *
 * We are attempting to connect to the Home Network
 *
 * This is just a transient state as we will either get
 * CONNECTED or have a CONNECTION/AUTH Failure.
 *
 */
static void event_normal_connecting(void *data)
{
	dbg("Connecting to Home Network");
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
WEAK void wlan_event_normal_connected(void *data)
{
	char ip[16];

	app_network_ip_get(ip);
	dbg("Connected to Home Network with IP address =%s", ip);
	led_on(board_led_2());
}

/*
 * Event: CONNECT_FAILED
 *
 * We attempted to connect to the Home AP, but the connection did
 * not succeed.
 *
 * This typically indicates:
 *
 * -- The access point could not be found.
 * -- We did not get a valid IP address from the AP
 *
 */
WEAK void wlan_event_normal_connect_failed()
{
	dbg("Application Error: Connection Failed");
	app_sta_stop();
}

/*
 * Event: LINK LOSS
 *
 * We lost connection to the AP.
 *
 * The App Framework will attempt to reconnect. We don't
 * need to do anything here.
 */
WEAK void wlan_event_normal_link_lost(void *data)
{
	dbg("Link Lost");
}

static void event_prov_done(void *data)
{
	app_provisioning_stop();
	dbg("Provisioning successful");
}

static void event_prov_client_done(void *data)
{
	app_uap_stop();
}

static void event_normal_reset_prov(void *data)
{
	provisioned = 0;
	if (is_uap_started() == false) {
		app_uap_start_with_dhcp(uap_nw.ssid, uap_nw.passphrase);
	} else {
		app_provisioning_start(PROVISIONING_WLANNW);
	}
	led_slow_blink(board_led_2());
}

/* This is the main event handler for this project. The application framework
 * calls this function in response to the various events in the system.
 */
static int wlan_common_event_handler(int event, void *data)
{
	switch (event) {
	case AF_EVT_INIT_DONE:
		event_init_done(data);
		break;
	case AF_EVT_PROV_DONE:
		event_prov_done(data);
		break;
	case AF_EVT_WLAN_INIT_DONE:
		event_wlan_init_done(data);
		break;
	case AF_EVT_NORMAL_CONNECTING:
		event_normal_connecting(data);
		break;
	case AF_EVT_NORMAL_CONNECTED:
		wlan_event_normal_connected(data);
		break;
	case AF_EVT_NORMAL_CONNECT_FAILED:
		wlan_event_normal_connect_failed(data);
		break;
	case AF_EVT_NORMAL_LINK_LOST:
		wlan_event_normal_link_lost(data);
		break;
	case AF_EVT_PROV_CLIENT_DONE:
		event_prov_client_done(data);
		break;
	case AF_EVT_NORMAL_RESET_PROV:
		event_normal_reset_prov(data);
		break;
	default:
		break;
	}

	return 0;
}

int wm_wlan_connect(char *ssid, char *passphrase)
{
	/* Populate station information */
	memset(&sta_nw, 0, sizeof(uap_nw));
	strncpy(sta_nw.ssid, ssid, sizeof(sta_nw.ssid));
	strncpy(sta_nw.passphrase, passphrase, sizeof(sta_nw.passphrase));

	/* Start the application framework */
	if (app_framework_start(wlan_common_event_handler) != WM_SUCCESS) {
		dbg("Failed to start application framework");
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

int wm_wlan_start(char *ssid, char *wpa2_passphrase)
{
	/* Populate uAP information */
	memset(&uap_nw, 0, sizeof(uap_nw));
	strncpy(uap_nw.ssid, ssid, IEEEtypes_SSID_SIZE);
	strncpy(uap_nw.passphrase, wpa2_passphrase, WLAN_PSK_MAX_LENGTH);

	/* Start the application framework */
	if (app_framework_start(wlan_common_event_handler) != WM_SUCCESS) {
		dbg("Failed to start application framework");
		return -WM_FAIL;
	}
	return WM_SUCCESS;
}
