/*
 *  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* app_reboot.c The following set of functions are used to set/get the time
 * after which the module will reset. This is used to cause a periodic reboot of
 * the module, something like a watchdog mechanism.
 *
 * This acts as a system watchdog. A configurable timeout duration is set in
 * psm. A timer can be started for this duration. If a strobe is not received
 * before the timer expires, a reboot is triggered.
 */


#include <wmstats.h>
#include <diagnostics.h>
#include <app_framework.h>

#include "app_dbg.h"
extern diagnostics_write_cb diag_write_cb;

/*
 * This function sets the global variables g_reboot_reason and g_reboot_flag and
 * stores diagnostics statistics to psm and triggers a reboot
 *
 */
int app_reboot(wm_reboot_reason_t reason)
{
	if (diag_write_cb)
		diag_write_cb(app_psm_hnd, reason);
	pm_reboot_soc();
	return WM_SUCCESS;
}

