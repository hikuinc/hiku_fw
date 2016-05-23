/*
 *  Copyright (C) 2008-2016 Marvell International Ltd.
 *  All Rights Reserved.
 */


#include <wm_os.h>
#include <wm_utils.h>
#include <stdlib.h>

#define THREAD_COUNT 2
static os_thread_stack_define(test_thread_stack1, 512);
static os_thread_stack_define(test_thread_stack2, 512);
static os_thread_t test_thread_handle[THREAD_COUNT];
static int test_count;
static os_semaphore_t semaphore;

/*
 *  Name: thread_func2
 *
 *  Description: After getting semaphore it increments the count
 *  value and puts the semaphore and places the os thread to sleep
 *  and comes out after deleting the thread.
 *
 */

static void thread_func2(void *arg)
{
	int thread_index = (int) arg;
	while (1) {
		wmprintf("Waiting to hold the semaphore(thread2)\r\n");
		os_semaphore_get(&semaphore, OS_WAIT_FOREVER);
		wmprintf("Eureka!! Got it\r\n");
		wmprintf("Count: %d \r\n", test_count);
		test_count++;
		wmprintf("Increment count: %d \r\n", test_count);
	}
	test_thread_handle[thread_index] = NULL;
	os_thread_delete(NULL);
}

/*
 *  Name: thread_func1
 *
 *  Description: It releases the semaphore and goes back to sleep.
 *
 */

static void thread_func1(void *arg)
{
	int thread_index = (int) arg;
	while (1) {
		os_semaphore_put(&semaphore);
		wmprintf("semaphore released(thread1)\r\n");
		os_thread_sleep(os_msec_to_ticks(3000));
	}
	test_thread_handle[thread_index] = NULL;
	os_thread_delete(NULL);
}

/*
 *  Name: semaphore_demo
 *
 *  Description: Creates two threads and semaphore and returns thread and
 *  semaphore creation status. The threads in turn calls the thread_func1 and
 *  thread_func2 functions
 *
 */

int semaphore_demo()
{
	int rv = os_semaphore_create(&semaphore, "sem-tst");
	if (rv != WM_SUCCESS) {
		wmprintf("%s: semaphore creation failed\r\n", __func__);
		return rv;
	}
	os_semaphore_get(&semaphore, OS_WAIT_FOREVER);
	wmprintf("Semaphore held by semaphore_demo\r\n");
	int index;
	index = 0;
	rv = os_thread_create(&test_thread_handle[index],
			      "thread1", thread_func1, (void *) index,
			      &test_thread_stack1,
			      2);

	if (rv != WM_SUCCESS) {
		wmprintf("%s: Thread creation failed\r\n", __func__);
		return rv;
	}
	index = 1;
	rv = os_thread_create(&test_thread_handle[index],
			      "thread2", thread_func2, (void *) index,
			      &test_thread_stack2,
			      1);

	if (rv != WM_SUCCESS) {
		wmprintf("%s: Thread creation failed\r\n", __func__);
		return rv;
	}
	return 0;
}
