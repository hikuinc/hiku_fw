/*
 *  Copyright (C) 2014, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* This project demonstrates power manager functionality and different PM modes
 * of 88MC200 SoC */

#include <wmstdio.h>
#include <wm_os.h>
#include <pwrmgr.h>
#include <wmtime.h>
#include <cli.h>
#include <stdlib.h>
#include <board.h>
#include <mdev_pm.h>

#include <flash.h>

int test_init()
{
	int ret = 0;
	wmstdio_init(UART0_ID, 0);
	cli_init();
	pm_init();
	wmtime_init();
	pm_mcu_cli_init();
	return ret;
}

/**
 * All application specific initialization is performed here
 */
int main(void)
{
	test_init();
	while (1) {
		os_thread_sleep(os_msec_to_ticks(1000));
	}
	return 0;
}
