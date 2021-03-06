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
#include <ttcp.h>
#include <dhcp-server.h>
#include <crc32.h>
#include <mrvlperf.h>
#include <mdev_i2c.h>
/* Generic Application includes */
#include <app_framework.h>

/* Application specific includes */
#include <perf_demo_app.h>
#include <perf_demo_drv.h>

char perf_demo_hostname[30];
char perf_demo_ssid[30];
#define MAX_RETRY_TICKS 50
static void perf_demo_configure_defaults()
{
	uint8_t my_mac[6];

	wlan_get_mac_address(my_mac);
	/* Provisioning SSID */
	snprintf(perf_demo_ssid, sizeof(perf_demo_ssid),
		 "wmdemo-%02X%02X", my_mac[4], my_mac[5]);
	/* Default hostname */
	snprintf(perf_demo_hostname, sizeof(perf_demo_hostname),
		 "wmdemo-%02X%02X", my_mac[4], my_mac[5]);
	wifi_set_packet_retry_count(MAX_RETRY_TICKS);
}


static void module_init()
{
	ttcp_init();
	healthmon_start();
	healthmon_register_cli();
	dhcpd_cli_init();
	rfget_cli_init();
}

/** This is the main entry point into the application. This is where any
 * initializations are performed. All user applications should begin by
 * initializing the core WM modules by calling wm_core_and_wlan_init.
 *
 * Finally, control is handed over to the Application Framework.
 *
 * Once you have read this, please go to perf_demo_configure_app_framework().
 *
 */
int main()
{
	int ret;
	/* Initialize WM core modules */
	ret = wm_core_and_wlan_init();
	if (ret) {
		/* System core init failed, just wait forever here */
		while (1)
			;
	}
	wmprintf("[perf_demo] Build Time: "__DATE__" "__TIME__"\r\n");

	/* Initialize all the modules required for perf-demo */
	module_init();

	perf_demo_configure_defaults();

	app_sys_register_upgrade_handler();
	app_sys_register_diag_handler();

	/* Start the application framework */
	if (app_framework_start(perf_demo_app_event_handler) != WM_SUCCESS) {
		wmprintf("Failed to start application framework\r\n");
		while (1)
			;
	}

	/* Set the final_about_to_die handler of the healthmon */
	healthmon_set_final_about_to_die_handler
		((void *)diagnostics_write_stats);

	return 0;
}
