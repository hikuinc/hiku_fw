/*
 *  Copyright (C) 2008-2015 Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 * OS demo-Read Write Lock demo(Readers priority)
 *
 *
 * Summary:
 * This demo is present to give developers a ready reference of using
 * This demo application depicts the usage of various OS APIs
 * 1. os_semaphore_get / os_semaphore_put
 * 2. os_mutex_get /os_mutex_put
 * 3. os_thread_create / os_thread_delete
 * 4. os_thread_sleep
 *
 * Readers Priority-Read Write lock.
 */
#include <wmstdio.h>
#include <wm_os.h>
#include "rw_lock_demo.h"
/*Function Declaration*/
void rp_rw_demo(void);
void rwl_delete(void);
void thread_delete(void);
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
	/*Readers Priority Read Write demo function call*/
	rp_rw_demo();
	/*Wait to get Semaphore until all Threads complete execution*/
	os_sem_get();
	wmprintf("All threads have finished execution\r\n");
	/*Free the allocated memory*/
	thread_delete();
	rwl_delete();
	return 0;
}
