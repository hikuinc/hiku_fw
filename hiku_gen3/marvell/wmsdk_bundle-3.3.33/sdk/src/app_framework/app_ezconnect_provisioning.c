/*
 *  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * app_ezconnect_provisioning.c: Functions that deal with the ezconnect
 * provisioning of the device.
 */

#include <wmstdio.h>
#include <wlan.h>
#include <provisioning.h>
#include <dhcp-server.h>
#include <app_framework.h>
#include <mdns.h>

#include "app_ctrl.h"
#include "app_dbg.h"
#include "app_provisioning.h"

enum app_smc_state {
	EZCONN_PROV_INIT,
	EZCONN_PROV_STARTED,
	EZCONN_PROV_STOPPED,
	EZCONN_UAP_BASED_PROV,
	EZCONN_SNIFFER_BASED_PROV,
} ezconn_prov_state;

static os_timer_t ezconnect_timer, prov_client_done_timer;
static uint8_t g_prov_key[PROV_DEVICE_KEY_MAX_LENGTH];
static int g_prov_key_len;
static struct provisioning_config pc;

#define SMC_MODE_TIMEOUT        (60 * 1000)
#define PROV_CLIENT_DONE_TIMEOUT     (60 * 1000)

int app_ezconn_prov_event_handler(enum prov_event event, void *arg,
				  int len);

static int app_ezconn_provisioning_get_type()
{
	return pc.prov_mode;
}

static int app_sniffer_provisioning_start()
{
	pc.prov_mode = PROVISIONING_EZCONNECT;
	pc.provisioning_event_handler = app_ezconn_prov_event_handler;

	if (prov_ezconnect_start(&pc) != WM_SUCCESS) {
		app_e("prov: failed to launch provisioning app.");
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

static void app_sniffer_provisioning_stop()
{
	prov_ezconnect_finish();
}

int app_ezconn_prov_event_handler(enum prov_event event, void *arg,
				  int len)
{
	switch (event) {
	case PROV_NETWORK_SET_CB:
		if (len != sizeof(struct wlan_network))
			return -WM_FAIL;
		app_ctrl_notify_event(AF_EVT_SNIFFER_NW_SET, arg);
		break;
	case PROV_MICRO_AP_UP_REQUESTED:
		if (len != sizeof(struct wlan_network))
			return -WM_FAIL;
		app_ctrl_notify_event(AF_EVT_SNIFFER_MICRO_UP_REQUESTED, arg);
		break;
	default:
		break;
	}
	return WM_SUCCESS;
}

struct mdns_service ezconn_service = {
	.servtype = "ezconnect-prov",
	.domain = "local",
	.proto = MDNS_PROTO_TCP,
	.port = 8080,
	.keyvals = "provisioned=1:path=/wm_demo:description=Wireless" \
	"Microcontroller:Product=WM1",
	.servname = "ezconnect",
};

static int app_ezconn_uap_based_prov_stop()
{
	int ret = WM_SUCCESS;
	if (os_timer_is_running(ezconnect_timer))
		ret = os_timer_deactivate(&ezconnect_timer);
	unregister_secure_provisioning_web_handlers();
	dhcp_server_stop();
	if (is_uap_started()) {
		app_uap_stop();
	}
	if (prov_client_done_timer) {
		os_timer_deactivate(&prov_client_done_timer);
		os_timer_delete(&prov_client_done_timer);
		prov_client_done_timer = NULL;
	}
	return ret;
}

#define MAX_BUF_SIZE 128
int app_ezconnect_sm(app_ctrl_event_t evt, void *data)
{
	int event = (int) evt;
	void *iface_handle;
	char ezconn_rec[MAX_BUF_SIZE];
	struct wlan_network home_nw;

	switch (ezconn_prov_state) {
	case EZCONN_PROV_INIT:
		if (event == AF_EVT_EZCONN_PROV_START) {
			/* This function starts smc mode and returns */
			app_sniffer_provisioning_start();
			app_ctrl_notify_event(AF_EVT_INTERNAL_PROV_REQUESTED,
					      (void *)NULL);
			ezconn_prov_state = EZCONN_PROV_STARTED;
		}
		break;
	case EZCONN_PROV_STARTED:
		switch (event) {
		case AF_EVT_SNIFFER_NW_SET:
			memcpy(&home_nw, data, sizeof(struct wlan_network));
			app_sta_save_network_and_start(&home_nw);
			if (os_timer_is_running(ezconnect_timer))
				os_timer_deactivate(&ezconnect_timer);
			app_sniffer_provisioning_stop();
			ezconn_prov_state = EZCONN_SNIFFER_BASED_PROV;
			break;
		case AF_EVT_SNIFFER_MICRO_UP_REQUESTED:
			/* micro AP is started immediately on receiving AUTH_REQ
			 * to avoid latency. Event is delivered immediately */
			app_sniffer_provisioning_stop();
			ezconn_prov_state = EZCONN_UAP_BASED_PROV;
			break;
		default:
			break;
		}
		break;
	case EZCONN_UAP_BASED_PROV:
		switch (event) {
		case AF_EVT_UAP_STARTED:
			if (dhcp_server_start(net_get_uap_handle()) !=
			    WM_SUCCESS)
				app_e("Failed to start dhcp server\r\n");
			register_secure_provisioning_web_handlers(g_prov_key,
				g_prov_key_len,
				app_sta_save_network_and_start);
			os_timer_activate(&ezconnect_timer);
			wscan_periodic_start();
			break;
		case AF_EVT_RESET_TO_SMC:
			app_ezconn_uap_based_prov_stop();
			break;
		case AF_EVT_UAP_STOPPED:
			/* This event is handled in state EZCONN_UAP_BASED_PROV
			 * only when micro-AP up timeout stops micro-AP. On
			 * successful micro-AP based provisioning, when
			 * PROV_CLIENT_DONE event is received, micro-AP is
			 * stopped and state is changed to EZCONN_PROV_STOPPED.
			 */
			if (wscan_periodic_stop() != WM_SUCCESS)
				app_e("Failed to dequeue wlan_scan from "
				      "system wq");
			app_sniffer_provisioning_start();
			ezconn_prov_state = EZCONN_PROV_STARTED;
			break;
		case AF_EVT_PROV_DONE:
			/* do nothing */
			break;
		case AF_EVT_NORMAL_RESET_PROV:
			app_ezconn_uap_based_prov_stop();
			break;
		case AF_EVT_PROV_CLIENT_DONE:
			app_ezconn_uap_based_prov_stop();
			ezconn_prov_state = EZCONN_PROV_STOPPED;
			break;
		default:
			break;
		}
		break;
	case EZCONN_SNIFFER_BASED_PROV:
		switch (event) {
		case AF_EVT_NORMAL_CONNECTED:
			iface_handle = net_get_sta_handle();
			app_mdns_start("ezconnect");
			snprintf(ezconn_rec, MAX_BUF_SIZE, "provisioned=%d", 1);
			mdns_set_txt_rec(&ezconn_service, ezconn_rec, ' ');
			app_mdns_add_service(&ezconn_service, iface_handle);
			register_secure_provisioning_web_handlers(g_prov_key,
				g_prov_key_len,
				app_sta_save_network_and_start);
			break;
		case AF_EVT_PROV_DONE:
			/* do nothing */
			break;
		case AF_EVT_NORMAL_RESET_PROV:
			app_ezconn_uap_based_prov_stop();
			break;
		case AF_EVT_PROV_CLIENT_DONE:
			app_mdns_stop();
			unregister_secure_provisioning_web_handlers();
			ezconn_prov_state = EZCONN_PROV_STOPPED;
			break;
		default:
			break;
		}
		break;
	case EZCONN_PROV_STOPPED:
		/* do nothing */
		break;
	default:
		break;
	}
	return WM_SUCCESS;
}

static void smc_mode_timer_cb()
{
	sta_list_t *sl;
	wifi_uap_bss_sta_list(&sl);
	if (sl->count)
		return;
	app_ctrl_notify_event(AF_EVT_RESET_TO_SMC, 0);
}

static void prov_client_done_timeout_cb()
{
	app_ctrl_notify_event(AF_EVT_PROV_CLIENT_DONE, 0);
}

int app_ezconnect_provisioning_start(char *ssid, uint8_t *prov_key,
				     int prov_key_len)
{
	if (prov_key_len < 1 || prov_key == NULL) {
		app_e("Invalid EZConnect provisioning key");
		return -WM_FAIL;
	}

	if (ssid == NULL) {
		app_e("Invalid micro-AP ssid");
		return -WM_FAIL;
	}

	if (prov_ezconn_set_device_key(prov_key, prov_key_len) != WM_SUCCESS)
		return -WM_FAIL;

	memcpy(g_prov_key, prov_key, sizeof(g_prov_key));
	g_prov_key_len = prov_key_len;

	register_ezconnect_provisioning_ssid(ssid);

	if (os_timer_create(&ezconnect_timer,
			    "ezconnect_timer",
			    os_msec_to_ticks(SMC_MODE_TIMEOUT),
			    &smc_mode_timer_cb,
			    NULL,
			    OS_TIMER_PERIODIC,
			    OS_TIMER_NO_ACTIVATE) != WM_SUCCESS) {
		app_e("Failed to create micro AP up timer");
		return -WM_FAIL;
	}

	ezconn_prov_state = EZCONN_PROV_INIT;
	/* Start the state machine */
	app_add_sm(app_ezconnect_sm);

	return app_ctrl_notify_event(AF_EVT_EZCONN_PROV_START, NULL);
}

void app_ezconnect_provisioning_stop()
{
	if (wscan_periodic_stop() != WM_SUCCESS)
		app_e("Failed to dequeue wlan_scan from system wq");

	if (app_ezconn_provisioning_get_type() == PROVISIONING_EZCONNECT)
		app_sniffer_provisioning_stop();

	if (ezconnect_timer) {
		os_timer_deactivate(&ezconnect_timer);
		os_timer_delete(&ezconnect_timer);
		ezconnect_timer = NULL;
	}

	if (os_timer_create(&prov_client_done_timer,
			    "prov_client_done_timeout",
			    os_msec_to_ticks(PROV_CLIENT_DONE_TIMEOUT),
			    &prov_client_done_timeout_cb,
			    NULL,
			    OS_TIMER_ONE_SHOT,
			    OS_TIMER_AUTO_ACTIVATE) != WM_SUCCESS) {
		app_e("Failed to create prov_client_done_timeout timer");
	}
}
