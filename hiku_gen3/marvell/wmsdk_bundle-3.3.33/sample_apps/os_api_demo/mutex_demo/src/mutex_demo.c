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
static os_mutex_t mutex;

/*
 *  Name: test_thread_increment
 *
 *  Description: After getting mutex lock it increments and puts the mutex and
 *  places the os thread to sleep and comes out after deleting the thread.
 *
 */

static void test_thread_increment(void *arg)
{
	int thread_index = (int) arg;
	while (1) {
		os_mutex_get(&mutex, OS_WAIT_FOREVER);
		wmprintf("Mutex lock being used by increment thread\r\n");
		wmprintf("Count: %d \r\n", test_count);
		test_count++;
		wmprintf("Increment count: %d \r\n", test_count);
		os_thread_sleep(os_msec_to_ticks(3000));
		test_count++;
		wmprintf("Increment count: %d \r\n", test_count);
		os_thread_sleep(os_msec_to_ticks(3000));
		wmprintf("Mutex unlocked by increment thread\r\n");
		os_mutex_put(&mutex);
	}
	test_thread_handle[thread_index] = NULL;
	os_thread_delete(NULL);
}

/*
 *  Name: test_thread_decrement
 *
 *  Description: It makes the thead sleep until increment thread is done. Then
 *  after getting mutex lock it decrements and puts the mutex and comes out
 *  after deleting the thread.
 *
 */

static void test_thread_decrement(void *arg)
{
	int thread_index = (int) arg;
	while (1) {
		os_thread_sleep(3000);
		os_mutex_get(&mutex, OS_WAIT_FOREVER);
		wmprintf("Mutex lock held by decrement thread\r\n");
		test_count--;
		wmprintf("Decrement count: %d \r\n", test_count);
		os_thread_sleep(os_msec_to_ticks(3000));
		test_count--;
		wmprintf("Decrement count: %d \r\n", test_count);
		os_thread_sleep(os_msec_to_ticks(3000));
		wmprintf("Mutex unlocked by decrement thread\r\n");
		os_mutex_put(&mutex);
	}
	test_thread_handle[thread_index] = NULL;
	os_thread_delete(NULL);
}

/*
 *  Name: mutex_demo
 *
 *  Description: Creates two threads and mutex and returns thread and mutex
 *  creation status. The threads in turn calls the increment and decrement
 *  functions
 *
 */

int mutex_demo()
{
	int rv = os_mutex_create(&mutex, "m-tst", 1);
	if (rv != WM_SUCCESS) {
		wmprintf("%s: mutex creation failed\r\n", __func__);
		return rv;
	}
	int index;
	index = 0;
	rv = os_thread_create(&test_thread_handle[index],
			      "thread1", test_thread_decrement, (void *) index,
			      &test_thread_stack1,
			      2);
	if (rv != WM_SUCCESS) {
		wmprintf("%s: Thread creation failed\r\n", __func__);
		return rv;
	}
	index = 1;
	rv = os_thread_create(&test_thread_handle[index],
			      "thread2", test_thread_increment, (void *) index,
			      &test_thread_stack2,
			      1);

	if (rv != WM_SUCCESS) {
		wmprintf("%s: Thread creation failed\r\n", __func__);
		return rv;
	}
	return 0;
}
