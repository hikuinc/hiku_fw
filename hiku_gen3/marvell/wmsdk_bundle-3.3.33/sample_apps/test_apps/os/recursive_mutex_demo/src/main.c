/*
 *  Copyright (C) 2008-2015 Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 * OS demo
 *
 * Summary:
 * This demo is present to give developers a ready reference of using
 * various OS abstraction layer APIs.
 */

#include <wmstdio.h>
#include <wm_os.h>

int mutex_demo(void);

/*
 * The application entry point is main(). At this point, minimal
 * initialization of the hardware is done, and also the RTOS is
 * initialized.
 */
int main(void)
{
	/* Initialize console on uart0 */
	wmstdio_init(UART0_ID, 0);

	wmprintf("OS demo started\r\n");

	mutex_demo();
	return 0;
}
