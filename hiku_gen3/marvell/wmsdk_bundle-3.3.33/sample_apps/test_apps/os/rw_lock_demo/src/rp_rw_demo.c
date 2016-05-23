/*
 *  Copyright (C) 2008-2015 Marvell International Ltd.
 *  All Rights Reserved.
 */
#include "rw_lock_demo.h"
static os_thread_stack_define(test_thread_stack, 512);
#define THREAD_COUNT 5
static os_thread_t test_thread_handle[THREAD_COUNT];
/*Test count for critical section*/
static int test_count;
/*Read Write Lock Object declared*/
struct rw_lock_t *rw_lock;
/*Function to sleep for random time*/
static int random_sleep_time()
{
	int random = rand() % 128;
	return os_msec_to_ticks(((int)random));
}
/*Function to get thread priority randomly*/
static int get_random_thread_priority()
{
	return rand() % OS_PRIO_0;
}
/*Reader thread function*/
static void reader_thread(void *arg)
{
	int thread_index = (int) arg;
	/*Capture the read lock*/
	rwlock_read_lock(rw_lock);
	/*Critical Section Start*/
	wmprintf("Test Count in Reader Thread %d is %d\r\n",
		 thread_index, test_count);
	os_thread_sleep(random_sleep_time());
	/*Critical Section End*/
	/*Leave the read lock*/
	rwlock_read_unlock(rw_lock);
	/*Put the semaphore to indicate thread execution completion*/
	os_sem_put(thread_index);
	/*Self complete the thread*/
	os_thread_self_complete(NULL);
}
/*Writer thread function*/
static void writer_thread(void *arg)
{
	int thread_index = (int) arg;
	/*Capture the write lock*/
	rwlock_write_lock(rw_lock);
	/*Critical Section Start*/
	test_count++;
	wmprintf("Test Count in Writer Thread %d is %d\r\n",
		 thread_index, test_count);
	os_thread_sleep(random_sleep_time());
	/*Critical Section End*/
	/*Leave the write lock*/
	rwlock_write_unlock(rw_lock);
	/*Put the semaphore to indicate thread execution completion*/
	os_sem_put(thread_index);
	/*Self complete the thread*/
	os_thread_self_complete(NULL);
}
/*Reader priority thread creation function*/
void rp_rw_demo()
{
	/*Allocate memory to rwlock object*/
	rw_lock = (rw_lock_t *)os_mem_alloc(sizeof(rw_lock_t));
	/*Initiate the rwlock*/
	rwlock_init(rw_lock);
	int index;
	char name_buf[20];
	/*Create Alternate Read and Write Threads*/
	for (index = 0; index < THREAD_COUNT; index++) {
		snprintf(name_buf, sizeof(name_buf), "Thread-%d", index);
		if (index % 2 == 0)
			os_thread_create(&test_thread_handle[index],
					 name_buf, writer_thread,
					 (void *) index,
					 &test_thread_stack,
					 get_random_thread_priority());
		else
			os_thread_create(&test_thread_handle[index],
					 name_buf, reader_thread,
					 (void *) index,
					 &test_thread_stack,
					 get_random_thread_priority());
	}
}
/*Function to deallocate memory*/
void rwl_delete()
{
	rwlock_dinit(rw_lock);
}
/*Function to delete created threads*/
void thread_delete()
{
	int thread_index;
	for (thread_index = 0; thread_index < THREAD_COUNT; thread_index++) {
		os_thread_delete(&test_thread_handle[thread_index]);
	}
}
