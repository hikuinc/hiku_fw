/*
 *  Copyright (C) 2008-2016 Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 * Timer usage demo
 *
 * Summary:
 *
 * This application shows usage of  timer and its features
 *
 * Description:
 *
 * Timers can be programmed to execute periodically or
 * one shot. This application demonstrates both types of timer
 * execution i.e. one shot and periodic
 *
 */

#include <wmstdio.h>
#include <wm_os.h>

extern int periodic_timer_fn();

/*
 * The application entry point is main(). At this point, minimal
 * initialization of the hardware is done, and also the RTOS is
 * initialized.
 */
int main(void)
{
	/* Initialize console on uart0 */
	wmstdio_init(UART0_ID, 0);
	wmprintf("Timer demo started\r\n");
	periodic_timer_fn();
	return 0;
}
