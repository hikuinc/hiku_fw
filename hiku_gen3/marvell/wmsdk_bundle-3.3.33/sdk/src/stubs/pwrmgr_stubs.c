/*
 * Copyright 2008-2015, Marvell International Ltd.
 * All Rights Reserved.
 */

/* pwrmgr_stubs.c: Power Manager stubs functions
 */
#include <stdlib.h>

#include <wmstdio.h>
#include <cli.h>
#include <cli_utils.h>
#include <wmerrno.h>
#include <mdev_rtc.h>
#include <pwrmgr.h>
#include <rtc.h>
#include <wm_os.h>

/* This value can be changed as per requirement*/
#define MAX_CLIENTS 10
/**
   This structure holds callback function
   and action for which callback is to be involved
*/
typedef struct {
	unsigned char action;
	power_save_state_change_cb_fn cb_fn;
	void *data;
} power_state_change_cb_t;

int pm_register_cb(unsigned char action, power_save_state_change_cb_fn cb_fn,
		   void *data)
{
	return -WM_FAIL;
}

int pm_change_event(int handle, unsigned char action)
{
	return -WM_FAIL;
}

int pm_deregister_cb(int handle)
{
	return -WM_FAIL;
}

void pm_invoke_ps_callback(power_save_event_t action)
{
}

inline int wakelock_get(const char *id_str)
{
	return 0;
}

inline int wakelock_put(const char *id_str)
{
	return 0;
}

inline int wakelock_isheld(void)
{
	return 0;
}


void pm_reboot_soc()
{
#ifdef CONFIG_HW_RTC
	hwrtc_time_update();
#endif
	arch_reboot();
}


int pm_init(void)
{
	return -WM_FAIL;
}

bool is_pm_init_done()
{
	return false;
}
