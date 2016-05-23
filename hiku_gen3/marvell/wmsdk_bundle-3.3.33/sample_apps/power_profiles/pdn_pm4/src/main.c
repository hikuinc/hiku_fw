/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Power profile : Wi-Fi PDN + PM4
 *
 * Summary:
 * This application is a demo which illustrates
 * Wi-Fi PDN and MCU PM4 power profile
 *
 * Description:
 * This application contains following actions
 * -# Device boot-up
 * -# Switches to 32 MHz clock from 200 MHz
 * -# Wi-Fi is put in PDN
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

static void wifi_poweroff()
{
	/* Deinit Wlan */
	wlan_deinit();

	/* Disable Interrupts */
	PMU_WakeupSrcIntMask(PMU_WAKEUP_WLAN, MASK);
	NVIC_DisableIRQ(WIFIWKUP_IRQn);

	/* Change System Clock source to RC32M */
	CLK_SystemClkSrc(CLK_RC32M);

	/* On Wifi Poweroff system, CPU runs @32M
	 * SFLL not required */
	/* FIXME: If required keep board_cpu_freq() unchanged */
	CLK_SfllDisable();

	/* Switch off XTAL */
	CLK_RefClkDisable(CLK_XTAL_REF);
	/* Reconfigure SysTick config to run at 32M */
	SysTick->LOAD =  (32000000 / configTICK_RATE_HZ) - 1UL;
	SysTick->VAL = 0;

	/* Wifi Poweroff */
	PMU_PowerDownWLAN();
}

static int pdn_pm4_app_event_handler(int event, void *data)
{
	switch (event) {
	case AF_EVT_WLAN_INIT_DONE:
		wifi_poweroff();
		wmprintf("Entering PM4\r\n");
		/* This delay has been introduced for
		   the print to get executed. */
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
	if (app_framework_start(pdn_pm4_app_event_handler)
			!= WM_SUCCESS) {
		wmprintf("Failed to start application framework\r\n");
		while (1)
			;
	}
	return 0;
}
