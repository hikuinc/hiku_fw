/*
 *  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _APP_CTRL_H_
#define _APP_CTRL_H_

#include <app_framework.h>

/** These flags are required for maintaining diagnostics statistics.
 * These are set by the app_reboot function and are used from the
 * app_main thread to store diagnostics statistics to psm and trigger
 * a reboot.
 */
extern wm_reboot_reason_t g_reboot_reason;
extern uint8_t g_reboot_flag;

/** Init function for application controller */
int app_ctrl_init(void);

int app_ctrl_notify_event(app_ctrl_event_t event, app_ctrl_data_t *data);
int app_ctrl_notify_event_wait(app_ctrl_event_t event, void *data);
int app_ctrl_event_to_prj(app_ctrl_event_t event, void *data);
WEAK void app_handle_link_loss();
void app_handle_chan_switch();

struct connection_state {
	unsigned short auth_failed;
	unsigned short conn_success;
	unsigned short conn_failed;
	unsigned short dhcp_failed;
	unsigned char state;
};
extern struct connection_state conn_state;

extern app_event_handler_t g_ev_hdlr;
extern int app_reconnect_attempts;
#endif
