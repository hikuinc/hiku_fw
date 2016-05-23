/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Simple application using wlan_configure_listen_interval() and
 * wlan_configure_null_pkt_interval() APIs.
 *
 * Summary:
 *
 * An application which explains usage of  APIs
 * for configuring power save parameters.
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
#include <cli.h>
#include <critical_error.h>

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

/*
 * Handler invoked when WLAN subsystem is ready.
 *
 */
void event_wlan_init_done(void *data)
{
	int ret;

	/* For more detailed documentation
	 * please refer API documentation of
	 * these APIs
	 */
	wlan_configure_listen_interval(10);
	wlan_configure_null_pkt_interval(30);

	/*
	 * Add a network using wlan-add CLI and issue a wlan-connect
	 * request, using sniffer capture association request from
	 * MC200/MW300 to AP.
	 *
	 * In association request verify listen interval value.
	 */
	/*
	 * Initialize CLI Commands for WLAN modules:
	 *
	 * -- wlan: allows user to explore basic wlan functions
	 */

	ret = wlan_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_cli_init failed");

	ret = wlan_pm_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_pm_cli_init failed");

	wmprintf("#######################################################\r\n");
	wmprintf("Add a network using \"wlan-add\"\r\n");
	wmprintf("Connect to added network using \"wlan-connect\"\r\n");
	wmprintf("On successful connection issue \"pm-ieeeps-hs-cfg\""
		" to put wireless card in IEEE power save\r\n");
	wmprintf("Using sniffer verify that configured listen"
		" interval and null packet interval are used\r\n");
	wmprintf("#######################################################\r\n");
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

	ret = cli_init();

	if (ret != WM_SUCCESS) {
		dbg("Error: cli init failed.");
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
	 * Initialize Power Management Subsystem
	 */
	ret = pm_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: pm_init failed");
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

	ret = pm_mcu_cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: pm_mc200_cli_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	return;
}

int main()
{
	modules_init();

	dbg("Build Time: " __DATE__ " " __TIME__ "");

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		dbg("Failed to start application framework");
		critical_error(-CRIT_ERR_APP, NULL);
	} else {
		dbg("app_framework started\n");
	}
	return 0;
}
