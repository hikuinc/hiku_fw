/*
 *  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/** profiler_cli.c: The profiler CLI
 */

#include <cli.h>
#include <cli_utils.h>
#include <wmstdio.h>
#include <profiler.h>
#include "profiler-priv.h"

void prof_clear_cmd_cb(int argc, char **argv)
{
	prof_state_update(PROF_CLEAR);
}

struct cli_command prof_commands[] = {
	{"prof_clear", NULL, prof_clear_cmd_cb},
};

int prof_cli_init(void)
{
	int i;

	for (i = 0; i < sizeof(prof_commands) / sizeof(struct cli_command); i++)
		if (cli_register_command(&prof_commands[i]))
			return 1;
	return 0;
}


