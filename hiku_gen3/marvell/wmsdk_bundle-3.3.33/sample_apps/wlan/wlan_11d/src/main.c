/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Simple Application using wlan_enable_11d() API.
 *
 * Summary:
 *
 * This application shows a way to enable 11 d feature of WLAN
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

	/* In this method of configuration, the Wi-Fi driver of the SDK will
	 * scan for Access Points in the vicinity and accordingly configure
	 * itself to operate in the available frequency bands.
	 */
	wlan_enable_11d();
	/* In this method of configuration, the application defines up-front
	 * what is the country code that the MC200/MW300 is going to be
	 * deployed in. Once configured the Wi-Fi firmware obeys the configured
	 * countries regulations.
	 */
	/* wlan_set_country(COUNTRY_CN); */

	/* To test the above configuration enable Scan_debug from menuconfig
	 * located at Modules->WLAN->Wifi driver->Wifi extra debug options
	 * ->Enable extra debug (WIFI_EXTRA_DEBUG [=y])->Scan debug.
	 *
	 * Then add a network using wlan-add CLI and issue a wlan-connect
	 * request. On console verify channel numbers are set
	 * as per 11d or country configuration. In case of wlan_enable_11d()
	 * three scans will be observed in which first scan will start with
	 * passive mode on all channels. Then in second and third scan
	 * active scan will be observed on channel on which APs are found.
	 *
	 * In case of wlan_set_country(), channels are scanned as per country
	 * specification in active mode.
	 */

	/*
	 * Initialize CLI Commands for WLAN modules:
	 *
	 * -- wlan: allows user to explore basic wlan functions
	 */

	ret = wlan_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_cli_init failed");
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
