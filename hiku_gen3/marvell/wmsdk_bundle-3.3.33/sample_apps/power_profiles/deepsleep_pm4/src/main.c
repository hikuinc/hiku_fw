/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Power profile : Wi-Fi Deepsleep + PM4
 *
 * Summary:
 * This application is a demo which illustrates
 * Wi-Fi  Deepsleep and MCU PM4 power profile
 *
 * Description:
 * This application contains following actions
 * -# Device boot-up
 * -# Wi-Fi is put in  deepsleep
 * -# MCU enters PM4
 * -# Wakeup from PM4
 * -# Exit from PM4
 * -# Device reboots
 *
 * This application uses application framework.
 *
 */

/* System includes */
#include <wmstdio.h>
#include <string.h>
#include <wmtime.h>
#include <wm_os.h>
#include <wm_wlan.h>
#include <mdev_pm.h>
#include <stdlib.h>
#include <wm_net.h>
#include <wlan.h>

/* Generic Application includes */
#include <app_framework.h>

/* Time duration of pm4 is set as 60 seconds */
#define TIME_DUR		60000

static int deepsleep_pm4_app_event_handler(int event, void *data)
{
	switch (event) {
	case AF_EVT_WLAN_INIT_DONE:
		wlan_deepsleepps_on();
		wmprintf("Entering deepsleep\r\n");
		break;
	case AF_EVT_PS_ENTER:
		wmprintf("Entering PM4\r\n");
		/* This delay has been introduced
		   for the print to be displayed */
		os_thread_sleep(500);
		pm_mcu_state(PM4, TIME_DUR);
		break;
	default:
		break;
	}
	return 0;
}

int main()
{
	int ret;
	/* Initialize console on uart0 */
	wmstdio_init(UART0_ID, 0);
	wmprintf("UART Initialized\r\n");
	/* Initialize WM core modules */
	ret = wm_core_and_wlan_init();
	if (ret) {
		wmprintf("Error initializing WLAN subsystem. Reason: %d\r\n",
				ret);
		/* System core init failed, just wait forever here */
		while (1)
			;
	}
	/* Start the application framework */
	if (app_framework_start(deepsleep_pm4_app_event_handler)
			!= WM_SUCCESS) {
		wmprintf("Failed to start application framework\r\n");
		while (1)
			;
	}
	return 0;
}
