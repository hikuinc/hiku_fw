/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef PROFILER_PRIV_H
#define PROFILER_PRIV_H

#include <stdint.h>

struct ftrace {
	uint32_t fn_addr;
	uint8_t cnt;
} __attribute__ ((__packed__));

extern struct ftrace ftable[];

void prof_clear_cmd_cb(int argc, char **argv);

#endif
