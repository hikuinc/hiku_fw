/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* main.c
 * Entry point into the application specific code.
 */

/* System includes */
#include <wmstdio.h>
#include <string.h>
#include <wmtime.h>
#include <wm_os.h>
#include <wm_wlan.h>
#include <rfget.h>
#include <wmsysinfo.h>
#include <healthmon.h>
#include <diagnostics.h>
#include <dhcp-server.h>
#include <crc32.h>
#include <critical_error.h>
#include <led_indicator.h>

#include <mdev_i2c.h>
/* Generic Application includes */
#include <app_framework.h>

/* Application specific includes */

#include <app.h>



char cert_demo_hostname[30];
char cert_demo_ssid[30];

static void cert_demo_configure_defaults()
{
	uint8_t my_mac[6];

	wlan_get_mac_address(my_mac);
	/* Provisioning SSID */
	snprintf(cert_demo_ssid, sizeof(cert_demo_ssid),
		"demo-%02X%02X", my_mac[4], my_mac[5]);
	/* Default hostname */
	snprintf(cert_demo_hostname, sizeof(cert_demo_hostname),
		"demo-%02X%02X", my_mac[4], my_mac[5]);
}


static void module_init()
{
	sysinfo_init();
	healthmon_start();
	healthmon_register_cli();
}

/** This is the main entry point into the application. This is where any
 * initializations are performed. All user applications should begin by
 * initializing the core WM modules by calling wm_core_and_wlan_init.
 *
 * Finally, control is handed over to the Application Framework.
 *
 * Once you have read this, please go to cert_demo_configure_app_framework().
 *
 */
int main()
{
	int ret;


	/* Initialize WM core modules */
	ret = wm_core_and_wlan_init();
	if (ret) {
		wmprintf("Error in wlan_init %d\r\n", ret);
		/* System core init failed, just wait forever here */
		while (1)
			;
	}
	wmprintf("[demo] Build Time: "__DATE__" "__TIME__"\r\n");

	/* Initialize all the modules required for wm-demo */
	module_init();

	cert_demo_configure_defaults();

	app_sys_register_upgrade_handler();
	app_sys_register_diag_handler();

	/* Start the application framework */
	if (app_framework_start(cert_demo_event_handler) != WM_SUCCESS) {
		wmprintf("Failed to start application framework\r\n");
		while (1)
			;
	}

	/* Set the final_about_to_die handler of the healthmon */
	healthmon_set_final_about_to_die_handler
		((void *)diagnostics_write_stats);

	return 0;
}


void cricital_error(critical_errno_t errno, void *data)
{
	led_on(board_led_1());
	led_on(board_led_2());
	led_on(board_led_3());
	led_on(board_led_4());
}

