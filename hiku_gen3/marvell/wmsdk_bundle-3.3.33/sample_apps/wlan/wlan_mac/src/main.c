/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Simple Application using wlan_set_mac_addr() API.
 *
 * Summary:
 *
 * An application to demonstrate setting MAC address
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
 */
#include <wm_os.h>
#include <app_framework.h>
#include <wmstdio.h>
#include <wm_net.h>
#include <mdev_gpio.h>
#include <appln_cb.h>
#include <appln_dbg.h>
#include <httpd.h>
#include <dhcp-server.h>
#include <ftfs.h>
#include <rfget.h>
#include <cli_utils.h>
#include <critical_error.h>

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

/*
 * Handler invoked when WLAN subsystem is ready.
 *
 */
void event_wlan_init_done(void *data)
{
	uint8_t mac[6];

	wlan_get_mac_address(mac);

	wmprintf("#####################################\r\n");

	wmprintf("Get MAC Address: ");

	print_mac((const char *)mac);

	wmprintf("\r\n");

	wmprintf("#####################################\r\n");
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
	return;
}

int main()
{
	uint8_t mac[] = { 0x00, 0x50, 0x43, 0x21, 0x19, 0x6E};

	modules_init();

	dbg("Build Time: " __DATE__ " " __TIME__ "");

	wmprintf("#####################################\r\n");

	wmprintf("Set MAC address to: ");

	print_mac((const char *)mac);

	wmprintf("\r\n");

	wmprintf("#####################################\r\n");

	wlan_set_mac_addr(mac);

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		dbg("Failed to start application framework");
		critical_error(-CRIT_ERR_APP, NULL);
	} else {
		dbg("app_framework started\n");
	}
	return 0;
}
