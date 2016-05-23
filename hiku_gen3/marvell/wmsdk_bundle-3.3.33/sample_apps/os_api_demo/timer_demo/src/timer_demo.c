/*
 *  Copyright (C) 2008-2016 Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wm_os.h>
#include <wm_utils.h>
#include <stdlib.h>

/* One shot timer interval */
#define ONESHOT_TIMEOUT (3 * 1000)

/* Periodic timer interval */
#define PERIODIC_TIMEOUT (5 * 1000)


/* Timer callback function */
static os_timer_t periodic_timer;

static void one_shot_timer_cb(os_timer_arg_t arg)
{
	wmprintf("One Shot timer callback\r\n");
}

/* One shot timer creation */
int oneshot_timer_fn()
{
	os_timer_t one_shot_timer;
	/* Create one shot timer */
	wmprintf("One Shot Timer Created\r\n");
	int rv = os_timer_create(&one_shot_timer,
				 "oneshot-timer",
				 os_msec_to_ticks(ONESHOT_TIMEOUT),
				 &one_shot_timer_cb,
				 NULL,
				 OS_TIMER_ONE_SHOT,
				 OS_TIMER_NO_ACTIVATE);
	if (rv != WM_SUCCESS) {
		wmprintf("Unable to create timer");
		return rv;
	}
	wmprintf("Activate the timer\r\n");
	/* Activate one shot timer */
	os_timer_activate(&one_shot_timer);
	return 0;
}

/* Periodic timer callback function */
static void periodic_timer_cb(os_timer_arg_t arg)
{
	static int cnt = 1;

	wmprintf("Periodic  timer callback : %d\r\n", cnt++);
	if (cnt == 5)
		oneshot_timer_fn();
	if (cnt == 11) {
		os_timer_deactivate(&periodic_timer);
		wmprintf("Deactivating Periodic Timer\r\n");
	}
}

/* Periodic timer creation */
int periodic_timer_fn()
{
	wmprintf("Periodic Timer Created\r\n");
	/* Create a  periodic timer */
	int rv = os_timer_create(&periodic_timer,
				 "periodic-timer",
				 os_msec_to_ticks(PERIODIC_TIMEOUT),
				 &periodic_timer_cb,
				 NULL,
				 OS_TIMER_PERIODIC,
				 OS_TIMER_NO_ACTIVATE);
	if (rv != WM_SUCCESS) {
		wmprintf("Unable to  create timer");
		return rv;
	}
	wmprintf("Activate the timer\r\n");

	/* Activate periodic timer */
	os_timer_activate(&periodic_timer);
	return 0;
}
