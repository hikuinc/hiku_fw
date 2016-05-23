/*
 *  Copyright (C) 2008-2015 Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <wm_os.h>
#include <wm_utils.h>
#include <wmstdio.h>
#include <stdlib.h>

typedef struct rw_lock_t {
	int rcnt;
	os_mutex_t s_mutex;
	os_mutex_t r_mutex;
} rw_lock_t;

void rwlock_init(rw_lock_t *);
void rwlock_dinit(rw_lock_t *);
void rwlock_read_lock(rw_lock_t *);
void rwlock_read_unlock(rw_lock_t *);
void rwlock_write_lock(rw_lock_t *);
void rwlock_write_unlock(rw_lock_t *);
void os_sem_put(int);
void os_sem_get(void);
