/*
 *  Copyright (C) 2008-2016 Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 * Semaphore usage demo application
 *
 * Summary:
 *
 * This application demonstrates usage of semaphores for
 * synchronization between threads.
 *
 * Description:
 *
 * Semaphore being a common OS primitive used for
 * thread synchronization. This application is meant to
 * be an example for developers showing  semaphore usage.
 *
 */

#include <wmstdio.h>
#include <wm_os.h>

int semaphore_demo(void);

/*
 * The application entry point is main(). At this point, minimal
 * initialization of the hardware is done, and also the RTOS is
 * initialized.
 */
int main(void)
{
	/* Initialize console on uart0 */
	wmstdio_init(UART0_ID, 0);
	wmprintf("Semaphore demo started\r\n");
	semaphore_demo();
	return 0;
}
