/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _APP_H_
#define _APP_H_

/** Starts the application */
int cert_demo_app_start();
/** Stops the application */
int cert_demo_app_stop();
/** The event handler */
int cert_demo_event_handler(int event, void *data);

extern char cert_demo_hostname[];
extern char cert_demo_ssid[];

#ifdef DEMO_APP_DEBUG_PRINT
#define DEMO_APP_DBG(...)  wmprintf("[app-demo debug] " __VA_ARGS__)
#else
#define DEMO_APP_DBG(...)	(void)0
#endif

#define DEMO_APP_LOG(...)  wmprintf("[app-demo] " __VA_ARGS__)

#endif
