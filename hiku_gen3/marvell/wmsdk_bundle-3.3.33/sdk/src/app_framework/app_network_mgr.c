/*
 *  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* app_network_mgr.c : This file contains the functions that interact with the
 * wlcmgr like start, connect, etc. This contains the wlcmgr callback function
 * as well (for the normal mode), which transfers control over to the app_ctrl.
 */

#include <wmstdio.h>
#include <wlan.h>
#include <cli_utils.h>
#include <app_framework.h>
#include <wm_net.h>
#include <diagnostics.h>
#include <dhcp-server.h>

#include "app_ctrl.h"
#include "app_network_mgr.h"
#include "app_network_config.h"
#include "app_dbg.h"
#include "app_http.h"

int app_reconnect_attempts = 0;

static struct wlan_network *uap_network;
static int app_network_valid;
static bool app_network_dirty_flag;
static char wlan_started;

extern int (*dhcp_start_cb)(void *iface_handle);
extern void (*dhcp_stop_cb)();
extern bool sta_stop_cmd;

int app_check_valid_network_loaded()
{
	return app_network_valid;
}

int app_check_current_network_dirty()
{
	return app_network_dirty_flag;
}

void app_unset_current_network_dirty()
{
	app_network_dirty_flag = false;
}

void app_set_current_network_dirty()
{
	app_network_dirty_flag = true;
}

/* Connect to the default network */
int app_connect_wlan()
{
	int ret;
	/* Initiate connection to network */
	ret = wlan_connect(DEF_NET_NAME);
	if (ret != WM_SUCCESS) {
		app_w("network_mgr: failed to connect to network");
		return -WM_E_AF_NW_CONN;
	}
	return WM_SUCCESS;
}

int app_reconnect_wlan()
{
	app_reconnect_attempts++;
	app_d("network_mgr: reconnecting to network %d",
					app_reconnect_attempts);
	app_ctrl_notify_event(AF_EVT_NORMAL_CONNECTING, NULL);
	return app_connect_wlan();
}

/* Read the default network from the psm */
int app_load_configured_network()
{
	struct wlan_network app_network;

	if (app_network_valid) {
		app_l("network_mgr: network already loaded");
		return WM_SUCCESS;
	}

	/* Read network data to connect to */
	memset(&app_network, 0, sizeof(app_network));
	if (app_network_get_nw(&app_network) != 0) {
		app_e("network_mgr: failed to read network data");
		return -WM_E_AF_NW_RD;
	}

	if (wlan_add_network(&app_network) != WM_SUCCESS) {
		app_e("network_mgr: failed to add network");
		return -WM_E_AF_NW_ADD;
	}

	app_network_valid = 1;
	app_l("network_mgr: network loaded successfully");
	return WM_SUCCESS;
}

int app_load_this_network(struct wlan_network *network)
{
	if (app_network_valid) {
		app_l("network_mgr: network already loaded.");
		return WM_SUCCESS;
	}

	if (network == NULL)
		return -WM_FAIL;

	memcpy(network->name, DEF_NET_NAME, sizeof(network->name));

	int ret = wlan_add_network(network);
	if (ret != WLAN_ERROR_NONE) {
		app_e("network_mgr: failed to add network %d", ret);
		return -WM_E_AF_NW_ADD;
	}
	app_network_valid = 1;
	app_l("network_mgr: network loaded successfully");
	return WM_SUCCESS;
}

int app_unload_configured_network()
{

	if (!app_network_valid) {
		app_l("network_mgr: no network loaded. "
		      "Skipping unload");
		return WM_SUCCESS;
	}
	if (wlan_remove_network(DEF_NET_NAME) != WM_SUCCESS) {
		app_e("network_mgr: failed to remove network");
		return -WM_E_AF_NW_DEL;
	}

	app_network_valid = 0;
	app_l("network_mgr: network unloaded successfully");
	return WM_SUCCESS;
}

/* Callback Function passed to WLAN Connection Manager. The callback function
 * gets called when there are WLAN Events that need to be handled by the
 * application.
 */
int wlan_event_cb(enum wlan_event_reason reason, void *data)
{
	app_ctrl_data_t wlan_evt_data;

	app_d("network_mgr: WLAN: received event %d", reason);

	switch (reason) {
	case WLAN_REASON_INITIALIZED:
		app_d("network_mgr: WLAN initialized");
		app_ctrl_notify_event(AF_EVT_WLAN_INIT_DONE, NULL);
		break;
	case WLAN_REASON_INITIALIZATION_FAILED:
		app_e("network_mgr: WLAN: initialization failed");
		app_ctrl_notify_event(AF_EVT_INTERNAL_WLAN_INIT_FAIL, NULL);
		break;
	case WLAN_REASON_SUCCESS:
		app_d("network_mgr: WLAN: connected to network");
		/* Reset number of connection attempts */
		app_reconnect_attempts = 0;
		/* Notify Application Controller */
		app_ctrl_notify_event_wait(AF_EVT_NORMAL_CONNECTED,
					   NULL);
		break;
	case WLAN_REASON_CONNECT_FAILED:
		app_w("network_mgr: WLAN: connect failed");
		app_ctrl_notify_event(AF_EVT_NORMAL_CONNECT_FAILED, NULL);
		break;
	case WLAN_REASON_NETWORK_NOT_FOUND:
		app_w("network_mgr: WLAN: network not found");
		wlan_evt_data.app_data.fr = NETWORK_NOT_FOUND;
		app_ctrl_notify_event(AF_EVT_NORMAL_CONNECT_FAILED,
				      &wlan_evt_data);
		break;
	case WLAN_REASON_NETWORK_AUTH_FAILED:
		app_w("network_mgr: WLAN: network authentication failed");
		wlan_evt_data.app_data.fr = AUTH_FAILED;
		app_ctrl_notify_event(AF_EVT_NORMAL_CONNECT_FAILED,
				      &wlan_evt_data);
		break;
	case WLAN_REASON_ADDRESS_SUCCESS:
		app_d("network mgr: DHCP new lease\n\r");
		app_ctrl_notify_event(AF_EVT_NORMAL_DHCP_RENEW, NULL);
		break;
	case WLAN_REASON_ADDRESS_FAILED:
		app_w("network_mgr: failed to obtain an IP address");
		wlan_evt_data.app_data.fr = DHCP_FAILED;
		app_ctrl_notify_event(AF_EVT_NORMAL_CONNECT_FAILED,
				      &wlan_evt_data);
		break;
	case WLAN_REASON_WPS_DISCONNECT:
		app_w("network_mgr: wps disconnected");
		app_ctrl_notify_event(AF_EVT_NORMAL_WPS_DISCONNECT, NULL);
		break;
	case WLAN_REASON_USER_DISCONNECT:
		app_w("network_mgr: disconnected");
		sta_stop_cmd = false;
		app_ctrl_notify_event(AF_EVT_NORMAL_USER_DISCONNECT, NULL);
		break;
	case WLAN_REASON_LINK_LOST:
		app_w("network_mgr: WLAN: link lost");
		app_ctrl_notify_event(AF_EVT_NORMAL_LINK_LOST, NULL);
		break;
	case WLAN_REASON_CHAN_SWITCH:
		app_w("network_mgr: WLAN: channel switch");
		app_ctrl_notify_event(AF_EVT_NORMAL_CHAN_SWITCH, NULL);
		break;
	case WLAN_REASON_UAP_SUCCESS:
		app_d("network_mgr: WLAN: UAP Started");
		app_ctrl_notify_event(AF_EVT_UAP_STARTED, NULL);
		break;
	case WLAN_REASON_UAP_CLIENT_ASSOC:
		app_d("network_mgr: WLAN: UAP a Client Associated");
		memcpy(wlan_evt_data.app_data.uap_client_mac_address,
			data, MLAN_MAC_ADDR_LENGTH);
		app_ctrl_notify_event(AF_EVT_UAP_CLIENT_ASSOC, &wlan_evt_data);
		break;
	case WLAN_REASON_UAP_CLIENT_DISSOC:
		app_d("network_mgr: WLAN: UAP a Client Dissociated");
		memcpy(wlan_evt_data.app_data.uap_client_mac_address,
			data, MLAN_MAC_ADDR_LENGTH);
		app_ctrl_notify_event(AF_EVT_UAP_CLIENT_DISSOC, &wlan_evt_data);
		break;
	case WLAN_REASON_UAP_STOPPED:
		app_d("network_mgr: WLAN: UAP Stopped");
		app_ctrl_notify_event(AF_EVT_UAP_STOPPED, NULL);
		break;
	case WLAN_REASON_PS_ENTER:
		app_d("network_mgr: WLAN: PS_ENTER");
		wlan_evt_data.app_data.ps_data = (int) data;
		app_ctrl_notify_event(AF_EVT_PS_ENTER, &wlan_evt_data);
		break;
	case  WLAN_REASON_PS_EXIT:
		app_d("network_mgr: WLAN: PS EXIT");
		wlan_evt_data.app_data.ps_data = (int) data;
		app_ctrl_notify_event(AF_EVT_PS_EXIT, &wlan_evt_data);
		break;
	default:
		app_d("network_mgr: WLAN: Unknown Event: %d", reason);
	}
	return 0;
}

/* Start the wlcmgr, register the wlan_event_cb callback defined above. */
int app_start_wlan()
{
	int ret;

	if (wlan_started) {
		/* In case of certain scenarios where uAP and normal mode
		 * both are under the control of application framework, there
		 * is no need to stop and start wlan again. But wlan_start is
		 * required while starting the normal mode. So just send
		 * WLAN_REASON_INITIALIZED event to create an illusion to
		 * follow the normal path thereafter.
		 */
		wlan_event_cb(WLAN_REASON_INITIALIZED, NULL);
		return WM_SUCCESS;
	}

	ret = wlan_start(wlan_event_cb);
	if (ret) {
		app_e("netwokr_mgr: failed to start WLAN CM, "
			"status code %d\r\n", ret);
		return -WM_FAIL;
	}
	wlan_started = 1;
	return ret;
}

/* Stop the wlcmgr */
int app_stop_wlan()
{
	wlan_stop();
	wlan_started = 0;
	return 0;
}

/* Added this API which can be called on receiving micro AP started event in af.
 * uap_network holds micro-AP network information. When uAP is started without
 * notifying af, uap_network does not hold valid network information. Hence,
 * this API updates the uap_network.
 * Note: This API is required on enabling EZConnect provisioning
 */
int app_get_current_uap_network()
{
	int ret = WM_SUCCESS;
	if (!uap_network) {
		uap_network = os_mem_alloc(sizeof(struct wlan_network));
		if (!uap_network)
			return -WM_E_NOMEM;
		ret = wlan_get_current_uap_network(uap_network);
	}
	return ret;
}

int app_network_mgr_start_uap()
{
	int ret;

	if (!uap_network)
		return -WM_FAIL;

	wlan_remove_network(uap_network->name);

	ret = wlan_add_network(uap_network);
	if (ret != WM_SUCCESS) {
		return -WM_FAIL;
	}
	ret = wlan_start_network(uap_network->name);
	if (ret != WM_SUCCESS) {
		return -WM_FAIL;
	}
	return WM_SUCCESS;
}

int app_network_mgr_remove_uap_network()
{
	if (!uap_network)
		return -WM_FAIL;
	wlan_remove_network(uap_network->name);
	os_mem_free(uap_network);
	uap_network = NULL;
	return WM_SUCCESS;
}
int app_network_mgr_stop_uap()
{

	if (!uap_network)
		return -WM_FAIL;

	if (wlan_stop_network(uap_network->name) != WM_SUCCESS) {
		return -WM_FAIL;
	}
	return WM_SUCCESS;
}

int app_uap_start_by_network(struct wlan_network *net)
{
	if (uap_network)
		app_network_mgr_remove_uap_network();

	uap_network = os_mem_alloc(sizeof(struct wlan_network));
	memcpy(uap_network, net, sizeof(struct wlan_network));
	app_ctrl_notify_event(AF_EVT_INTERNAL_UAP_REQUESTED, NULL);
	return WM_SUCCESS;
}

#define MIN_WPA2_PASS_LEN  8
int app_uap_start(char *ssid, char *wpa2_pass)
{
	return app_uap_start_on_channel(ssid, wpa2_pass, 0);
}

int app_uap_start_on_channel(const char *ssid, const char *wpa2_pass,
				int channel)
{
	struct wlan_network u_net;
	wlan_initialize_uap_network(&u_net);
       /* Set SSID as passed by the user */
	strncpy(u_net.ssid, ssid, sizeof(u_net.ssid));
       /* Channel 0 sets channel selection to auto */
	u_net.channel = channel;
	/* If a passphrase is passed, set the security to WPA2, else set it to
	 * None.
	 */
	if (wpa2_pass) {
		if (strlen(wpa2_pass) < MIN_WPA2_PASS_LEN)
			return -EINVAL;
		u_net.security.type = WLAN_SECURITY_WPA2;
		/* Set the passphrase */
		strncpy(u_net.security.psk, wpa2_pass,
			sizeof(u_net.security.psk));
		u_net.security.psk_len = strlen(u_net.security.psk);
	} else {
		u_net.security.type = WLAN_SECURITY_NONE;
	}
	dhcp_start_cb = NULL;
	dhcp_stop_cb = NULL;
	return app_uap_start_by_network(&u_net);
}

int app_uap_stop()
{
	app_ctrl_notify_event(AF_EVT_INTERNAL_UAP_STOP, NULL);
	dhcp_start_cb = NULL;
	return WM_SUCCESS;
}

int app_uap_start_with_dhcp(const char *ssid, const char *wpa2_pass)
{
	return app_uap_start_on_channel_with_dhcp(ssid, wpa2_pass, 0);
}

int app_uap_start_on_channel_with_dhcp(const char *ssid, const char *wpa2_pass,
					int channel)
{
	if (app_uap_start_on_channel(ssid, wpa2_pass, channel) == WM_SUCCESS) {
		dhcp_start_cb = dhcp_server_start;
		dhcp_stop_cb = dhcp_server_stop;
		return WM_SUCCESS;
	}
	return -WM_FAIL;
}

int app_sta_save_network_and_start(struct wlan_network *net)
{
	app_ctrl_data_t *wlan_evt_data = os_mem_alloc(sizeof(app_ctrl_data_t));

	if (!wlan_evt_data) {
		return -WM_E_NOMEM;
	}

	wlan_evt_data->app_data.current_net = *net;
	wlan_evt_data->ptype = APP_DEFAULT_PROVISIONING;
	app_sta_save_network_and_start_with_prov_type(wlan_evt_data);
	os_mem_free(wlan_evt_data);

	return WM_SUCCESS;
}

int app_sta_save_network_and_start_with_prov_type(
	app_ctrl_data_t *wlan_evt_data)
{
	app_ctrl_notify_event(AF_EVT_NW_SET, wlan_evt_data);
	return WM_SUCCESS;
}

extern void app_set_state_configured();
int app_sta_save_network(struct wlan_network *net)
{
	int ret = app_network_set_nw(net);
	if (ret != WM_SUCCESS) {
		app_d("network_mgr: failed to set the network "
		      "in Persistent Storage");
		return -WM_FAIL;
	}

	/* At this point, the new network stored in PSM is not required
	   to be loaded. The below call is particularly required
	   because no connection attempts are made by the AF if
	   it is in UNCONFIGURED state unless the new network is loaded.
	   So, changing the AF state to the CONFIGURED state where
	   that check is not required and the new network is loaded
	   later in CONNECTING state */
	app_set_state_configured();
	/* Set the dirty flag since the current loaded network is no
	   longer valid. And, subsequent network connections should
	   be made with the new network configuration saved in PSM
	   using this api */
	app_set_current_network_dirty();

	return WM_SUCCESS;
}

int app_sta_start()
{
	/* Clear sta_stop_cmd flag before starting station */
	if (sta_stop_cmd)
		sta_stop_cmd = false;
	app_ctrl_notify_event(AF_EVT_INTERNAL_STA_REQUESTED, NULL);
	return WM_SUCCESS;
}

int app_sta_start_by_network(struct wlan_network *net)
{
	int ret;
	if (net == NULL)
		return -WM_FAIL;
	if (app_network_valid)
		app_unload_configured_network();
	ret = app_load_this_network(net);
	if (ret != WM_SUCCESS)
		return ret;
	/* Clear sta_stop_cmd flag before starting station */
	if (sta_stop_cmd)
		sta_stop_cmd = false;
	app_ctrl_notify_event(AF_EVT_INTERNAL_STA_REQUESTED, NULL);
	return WM_SUCCESS;
}

