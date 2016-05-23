/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* Advanced Event Handlers for perf_demo
 * This displays various status messages on the LCD screen connected to the
 * debug board.
 */


#include <wmstdio.h>
#include <app_framework.h>
#include <mdev.h>
#include <fs.h>
#include <ftfs.h>
#include <psm.h>
#include <dhcp-server.h>
#include "board.h"
#include "wm_os.h"
#include <wm_net.h>
#include "perf_demo_app.h"
#include "perf_demo_drv.h"
#include <led_indicator.h>

unsigned int provisioned;


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
 * perf_demo_http_set_lcd().
 */
int perf_demo_app_event_handler(int event, void *data)
{
	char ip[16];
	int i, err;
	struct fs *fs;
	struct app_init_state *state;

	PERF_DEMO_APP_LOG("perf_demo app: Received event %d\r\n", event);

	switch (event) {
	case AF_EVT_INIT_DONE:
		/* Register psm cli commands */
		err = psm_cli_init();
		if (err) {
			PERF_DEMO_APP_LOG("psm: failed to int PSM CLI,"
					"status code %d", err);
		}
		/* Register wlan cli commands */
		err = wlan_cli_init();
		if (err) {
			PERF_DEMO_APP_LOG("wlan: failed to int WLAN CLI,"
					  "status code %d", err);
		}
		/* The WM is up, perform any notification and initialization. */
		perf_demo_init_backlight();
		state = (struct app_init_state *)data;
		/* applications can use these to have their own set of
		 * indications */
		PERF_DEMO_APP_LOG("Factory reset bit status: %d\n\r",
				state->factory_reset);
		PERF_DEMO_APP_LOG("Booting from backup firmware status: %d\n\r",
				state->backup_fw);
		PERF_DEMO_APP_LOG("Previous reboot cause: %u\n\r",
				state->rst_cause);
		break;
	case AF_EVT_WLAN_INIT_DONE:
		i = (int) data;
		if (i == APP_NETWORK_NOT_PROVISIONED) {
			/* Include indicators if any */
			app_uap_start(perf_demo_ssid, UAP_SECURITY_PASSPHRASE);
			led_on(board_led_2());
			provisioned = 0;
		} else {
			provisioned = 1;
			app_uap_start(perf_demo_ssid, UAP_SECURITY_PASSPHRASE);
			app_sta_start();
		}
		break;
	case AF_EVT_PROV_DONE:
		/* Provisioning is complete, attempting to connect to
		 * network. Include some progress indicators while the
		 * connection attempt either succeeds or fails
		 */
		app_provisioning_stop();
		break;
	case AF_EVT_NORMAL_CONNECTING:
		/* Connecting attempt is in progress */
		net_dhcp_hostname_set(perf_demo_hostname);
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
		struct wlan_network curr_network;
		int rc = wlan_get_current_network
			(&curr_network);
		if (rc != WM_SUCCESS) {
			wmprintf(" Error in HS : "
				 "Cannot get network");
			return rc;
		}
		uint32_t ipv4 = curr_network.ip.ipv4.address;
		char buf[65];
		snprintf(buf, sizeof(buf), "%s",
			 inet_ntoa(ipv4));
		wmprintf(" IP Address:%s\r\n", buf);
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
	case AF_EVT_UAP_STARTED:
	{
		void *interface;
		interface = net_get_uap_handle();
		if (dhcp_server_start(interface)) {
			wmprintf("Error in starting dhcp server\r\n");
			return -1;
		}

		/* Start HTTPD */
		if (app_httpd_with_fs_start(FTFS_API_VERSION,
					    FTFS_PART_NAME, &fs) != WM_SUCCESS)
			wmprintf("Failed to start HTTPD\r\n");
		ftfs_cli_init(fs);
		if (!provisioned)
			app_provisioning_start(PROVISIONING_WLANNW |
					       PROVISIONING_WPS);
	}
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
	/* Ignoring these events for perf_demo, but other applications can use
	 * them accordingly
	 */
	case APP_CTRL_EVT_MAX:
	case AF_EVT_NORMAL_INIT:
	default:
		PERF_DEMO_APP_DBG("perf_demo app: Not handling event %d\r\n",
				event);
		break;
	}
	return 0;
}
