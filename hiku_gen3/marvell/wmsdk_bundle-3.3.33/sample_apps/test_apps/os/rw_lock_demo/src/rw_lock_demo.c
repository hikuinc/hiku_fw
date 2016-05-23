/*
 *  Copyright (C) 2008-2015 Marvell International Ltd.
 *  All Rights Reserved.
 */
#include "rw_lock_demo.h"
/*Semaphore declaration*/
os_semaphore_t sem[5];
/*Init function for mutex and semaphores*/
void rwlock_init(rw_lock_t *rw_lock)
{
	rw_lock->rcnt = 0;
	int rv, i;
	rv = os_mutex_create(&(rw_lock->r_mutex), "Read Mutex", 1);
	if (rv != WM_SUCCESS)
		wmprintf("%s: mutex creation failed\r\n", __func__);
	rv = os_mutex_create(&(rw_lock->s_mutex), "Section Mutex", 1);
	if (rv != WM_SUCCESS)
		wmprintf("%s: mutex creation failed\r\n", __func__);
	for (i = 0; i < 5; i++) {
		rv = os_semaphore_create_counting(&sem[i], "Sem1", 1, 0);
		if (rv != WM_SUCCESS)
			wmprintf("%s: Semaphore creation failed\r\n", __func__);
	}
}
/*Delete Function*/
void rwlock_dinit(rw_lock_t *rw_lock)
{
	int i;
	for (i = 0; i < 5; i++) {
		os_semaphore_delete(&sem[i]);
	}
	os_mutex_delete(&(rw_lock->r_mutex));
	os_mutex_delete(&(rw_lock->s_mutex));
	os_mem_free(rw_lock);
}
/*Read lock function*/
void rwlock_read_lock(rw_lock_t *rw_lock)
{
	os_mutex_get(&(rw_lock->r_mutex), OS_WAIT_FOREVER);
	(rw_lock->rcnt)++;
	if ((rw_lock->rcnt) == 1)
		os_mutex_get(&(rw_lock->s_mutex), OS_WAIT_FOREVER);
	os_mutex_put(&(rw_lock->r_mutex));
}
/*Read unlock function*/
void rwlock_read_unlock(rw_lock_t *rw_lock)
{
	os_mutex_get(&(rw_lock->r_mutex), OS_WAIT_FOREVER);
	(rw_lock->rcnt)--;
	if ((rw_lock->rcnt) == 0)
		os_mutex_put(&(rw_lock->s_mutex));
	os_mutex_put(&(rw_lock->r_mutex));
}
/*Write lock function*/
void rwlock_write_lock(rw_lock_t *rw_lock)
{
	os_mutex_get(&(rw_lock->s_mutex), OS_WAIT_FOREVER);
}
/*Write unlock function*/
void rwlock_write_unlock(rw_lock_t *rw_lock)
{
	os_mutex_put(&(rw_lock->s_mutex));
}
/*Semaphore put function*/
void os_sem_put(int thread_index)
{
	os_semaphore_put(&sem[thread_index]);
}
/*Semaphore get function*/
void os_sem_get(void)
{
	int i;
	for (i = 0; i < 5; i++) {
		os_semaphore_get(&sem[i], OS_WAIT_FOREVER);
	}
}
