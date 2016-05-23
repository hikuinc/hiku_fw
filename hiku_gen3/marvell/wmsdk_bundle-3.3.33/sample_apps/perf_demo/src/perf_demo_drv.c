
/*
 *  Copyright (C) 2012, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* perf_demo_drv.c
 * This file contains the driver for the sensor used in this sample perf_demo
 * application
 */

#include <stdio.h>
#include <wmstdio.h>
#include <string.h>
#include <wmtime.h>
#include <wm_os.h>
#include <board.h>
#include <critical_error.h>

#include "perf_demo_app.h"
#include "perf_demo_drv.h"
#include <led_indicator.h>


int perf_demo_get_time(int *hour, int *minute)
{
	struct tm date_time;
	wmtime_time_get(&date_time);
	*hour = (int)date_time.tm_hour;
	*minute = (int)date_time.tm_min;
	return 0;
}

int perf_demo_set_time(int hour, int minute)
{
	struct tm date_time;

	if (hour < 0 || hour > 23)
		return -1;

	if (minute < 0 || minute > 59)
		return -1;

	wmtime_time_get(&date_time);
	date_time.tm_hour = (char)hour;
	date_time.tm_min = (char)minute;
	wmtime_time_set(&date_time);

	return 0;
}

void cricital_error(critical_errno_t errno, void *data)
{
	led_on(board_led_1());
	led_on(board_led_2());
	led_on(board_led_3());
	led_on(board_led_4());
}
