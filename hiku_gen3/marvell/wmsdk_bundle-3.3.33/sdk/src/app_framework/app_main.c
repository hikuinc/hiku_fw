/*
 *  Copyright 2008-2015 Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wm_wlan.h>
#include "app_ctrl.h"
#include <critical_error.h>

app_event_handler_t g_ev_hdlr;

int app_framework_start(app_event_handler_t event_handler)
{
	int ret = wm_wlan_init();
	if (ret != WM_SUCCESS) {
		critical_error(-CRIT_ERR_WLAN_INIT_FAILED, NULL);
		return ret;
	}

	g_ev_hdlr = event_handler;
	return app_ctrl_init();
}
