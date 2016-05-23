/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* Advanced Event Handlers for demo
 * This displays various status messages on the LCD screen connected to the
 * debug board.
 */


#include <wmstdio.h>
#include <app_framework.h>
#include <mdev.h>
#include <psm.h>
#include <dhcp-server.h>
#include <telnetd.h>
#include <led_indicator.h>
#include "board.h"
#include "wm_os.h"
#include <wm_net.h>
#include "app.h"
#include "wlan.h"

unsigned int provisioned;

static char g_uap_ssid[IEEEtypes_SSID_SIZE + 1];
void cert_demo_http_register_handlers();

/* Security and passphrase for uAP mode */
#define UAP_SECURITY_TYPE	WLAN_SECURITY_WPA2
#define UAP_SECURITY_PASSPHRASE	"marvellwm"
#define FTFS_PART_NAME          "ftfs"

/** This is the main event handler for this project. The application framework
 * calls this function in response to the various events in the system.
 *
 * This function takes care of communicating with the various devices like the
 * LCD and LEDs as per the event received.
 *
 * Once you have read this, please look at the documentation for
 * cert_demo_http_set_lcd().
 */
int cert_demo_event_handler(int event, void *data)
{
	char ip[16];
	int i, err, ret;
	struct app_init_state *state;

	DEMO_APP_LOG("demo app: Received event %d\r\n", event);

	switch (event) {
	case AF_EVT_INIT_DONE:
		/* Register psm cli commands */
		err = psm_cli_init();
		if (err) {
			DEMO_APP_LOG("app_psm: failed to int PSM CLI,"
					"status code %d", err);
			return WM_FAIL;
		}
		/* The WM is up, perform any notification and initialization. */
		state = (struct app_init_state *)data;
		/* applications can use these to have their own set of
		 * indications */
		DEMO_APP_LOG("Factory reset bit status: %d\n\r",
				state->factory_reset);
		DEMO_APP_LOG("Booting from backup firmware status: %d\n\r",
				state->backup_fw);
		DEMO_APP_LOG("Previous reboot cause: %u\n\r",
				state->rst_cause);
		break;
	case AF_EVT_WLAN_INIT_DONE:
		i = (int) data;

		int err;
		/* Network services are started only once */
		err = app_httpd_start();
		if (err != WM_SUCCESS)
			wmprintf("Failed to start HTTPD\r\n");
		cert_demo_http_register_handlers();

		ret = wlan_cli_init();
		if (ret != WM_SUCCESS)
			DEMO_APP_LOG("Error: wlan_cli_init failed");

		ret = wlan_iw_cli_init();
		if (ret != WM_SUCCESS)
			DEMO_APP_LOG("Error: wlan_iw_cli_init failed");

		/* Include indicators if any */
		led_on(board_led_2());

		telnetd_start(23);

		if (i == APP_NETWORK_NOT_PROVISIONED) {
			uint8_t my_mac[6];

			wmprintf("\r\n\r\nThis device is not configured."
				 "Please set:\r\n");
			wmprintf("network.ssid=ssid\r\n");
			wmprintf("network.security=4\r\n");
			wmprintf("network.passphrase=passphrase\r\n");
			wmprintf("network.configured=1\r\n\n");
			wmprintf("For dynamic ip address assignment:\r\n");
			wmprintf("network.lan=DYNAMIC\r\n\n");
			wmprintf("For static ip address assignment:\r\n");
			wmprintf("network.lan=STATIC\r\n");
			wmprintf("network.ip_address=<ip_address>\r\n");
			wmprintf("network.gateway=<gateway>\r\n");
			wmprintf("network.subnet_mask=<subnet_mask>\r\n\n");
			provisioned = 0;

			memset(g_uap_ssid, 0, sizeof(g_uap_ssid));
			wlan_get_mac_address(my_mac);
			/* Provisioning SSID */
			snprintf(g_uap_ssid, sizeof(g_uap_ssid),
				 "certdemo-%02X%02X", my_mac[4], my_mac[5]);

			app_uap_start_with_dhcp(g_uap_ssid, NULL);
		} else {
			app_sta_start();
			provisioned = 1;
		}
		break;
	case AF_EVT_NORMAL_CONNECTING:
		/* Connecting attempt is in progress */
		net_dhcp_hostname_set(cert_demo_hostname);
		provisioned = 1;
		led_off(board_led_1());
		led_off(board_led_4());
		led_off(board_led_2());
		led_on(board_led_3());
		break;
	case AF_EVT_NORMAL_CONNECTED:
		/* We have successfully connected to the network. Note that
		 * we can get here after normal bootup or after successful
		 * provisioning.
		 */
		app_network_ip_get(ip);

		led_off(board_led_2());
		led_off(board_led_4());
		led_off(board_led_3());
		led_on(board_led_1());
		break;
	case AF_EVT_NORMAL_CONNECT_FAILED:
		/* One connection attempt to the network has failed. Note that
		 * we can get here after normal bootup or after an unsuccessful
		 * provisioning.
		 */
		led_off(board_led_1());
		led_off(board_led_4());
		led_off(board_led_3());
		led_on(board_led_2());
		break;
	case AF_EVT_NORMAL_LINK_LOST:
		/* We were connected to the network, but the link was lost
		 * intermittently.
		 */
		led_off(board_led_1());
		led_off(board_led_4());
		led_off(board_led_3());
		led_on(board_led_2());
		break;
	case AF_EVT_NORMAL_USER_DISCONNECT:
		led_off(board_led_1());
		led_off(board_led_4());
		led_off(board_led_3());
		led_on(board_led_2());
		break;
	case AF_EVT_PS_ENTER:
		wmprintf(" Power save enter\r\n");
		break;

	case AF_EVT_PS_EXIT:
		wmprintf(" Power save exit\r\n");
		break;
	case AF_EVT_NORMAL_RESET_PROV:
		/* Reset to provisioning
		*/
		led_off(board_led_1());
		for (i = 0; i < 5; i++) {
			led_on(board_led_2());
			os_thread_sleep(os_msec_to_ticks(500));
			led_off(board_led_2());
			os_thread_sleep(os_msec_to_ticks(500));
		}
	/* Ignoring these events for demo, but other applications can use
	 * them accordingly
	 */
	case APP_CTRL_EVT_MAX:
	case AF_EVT_NORMAL_INIT:
	default:
		DEMO_APP_DBG("demo app: Not handling event %d\r\n",
				event);
		break;
	}
	return 0;
}
