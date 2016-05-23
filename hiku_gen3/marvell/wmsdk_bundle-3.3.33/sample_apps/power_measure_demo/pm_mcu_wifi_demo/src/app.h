/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _PM_MCU_WIFI_DEMO_APP_H_
#define _PM_MCU_WIFI_DEMO_APP_H_

/** Starts the application */
int pm_mcu_wifi_demo_app_start();
/** Stops the application */
int pm_mcu_wifi_demo_app_stop();
/** The event handler */
int pm_mcu_wifi_demo_app_event_handler(int event, void *data);

extern char pm_mcu_wifi_demo_hostname[];
extern char pm_mcu_wifi_demo_ssid[];

#ifdef PM_MCU_WIFI_DEMOAPP_DEBUG_PRINT
#define PM_MCU_WIFI_DEMOAPP_DBG(...)  \
	wmprintf("[pm_mcu_wifi_demo debug] " __VA_ARGS__)
#else
#define PM_MCU_WIFI_DEMOAPP_DBG(...)	(void)0
#endif

#define PM_MCU_WIFI_DEMOAPP_LOG(...)  \
	wmprintf("[pm_mcu_wifi_demo] " __VA_ARGS__)

#endif
