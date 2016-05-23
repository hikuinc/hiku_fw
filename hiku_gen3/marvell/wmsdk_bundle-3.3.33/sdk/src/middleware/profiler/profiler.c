/*
 * Copyright 2008-2015, Marvell International Ltd.
 * All Rights Reserved.
 */

/** profiler.c: Function profiler. Provides count of each function.
 */
#include <stdio.h>
#include <profiler.h>
#include "profiler-priv.h"

struct ftrace ftable[CONFIG_PROFILER_FUNCTION_CNT];

void prof_clear_db()
{
	int i;
	for (i = 0; i < CONFIG_PROFILER_FUNCTION_CNT; i++) {
		ftable[i].fn_addr = 0;
		ftable[i].cnt = 0;
	}
}

int prof_state_update(prof_cmd_t prof_cmd)
{
	switch (prof_cmd) {
	case PROF_CLEAR:
		prof_clear_db();
		break;
	}
	return 0;
}

/* Function checks if address of called function is already present in the
 * table. If yes, increments its count. Else adds a new entry to the list.
 */
void mcount(uint32_t caller_lr, uint32_t callee_lr)
{
	volatile int i = 0, flag = 0;

/* For cortex-m3 and cortex-m4, thumb-2 instructions are used. Hence last bit
 * of LR is always 1. To get actual LR, subtract 1.
 */
	callee_lr -= 1;

/* Search address in a function table, if found, increment its count.
 * Else add a new entry.
 */
	for (i = 0; ftable[i].fn_addr != 0 && i < CONFIG_PROFILER_FUNCTION_CNT;
			i++) {
		if (ftable[i].fn_addr == callee_lr) {
			ftable[i].cnt += 1;
			flag = 1;
			break;
		}
	}
	if (flag == 0 && i < CONFIG_PROFILER_FUNCTION_CNT) {
		ftable[i].fn_addr = callee_lr;
		ftable[i].cnt = 1;
	}
}

/* This is a stub function included in the prologue of every C Function
 * when compiled with -pg switch. Function manually pushes all registers on to
 * the stack for local use and pops it back on exit.
 * Function checks if a called function is an ISR, if so, it simply returns,
 * otherwise calls mcount to generate its profile.
 */
void __gnu_mcount_nc()
{
	__asm volatile ("push {r0, r1, r2, r3, lr}");
	__asm volatile ("mrs r0, ipsr");
	__asm volatile ("cbnz r0, exit");
	__asm volatile ("subs r1, lr, #0");
	__asm volatile ("ldr r0, [sp, #20]");
	__asm volatile ("bl mcount");
	__asm volatile ("exit: pop {r0, r1, r2, r3, ip, lr}");
	__asm volatile ("bx ip");
}
