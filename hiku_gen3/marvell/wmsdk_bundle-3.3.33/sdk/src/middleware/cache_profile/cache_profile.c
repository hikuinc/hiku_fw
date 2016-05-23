/*
 * Copyright 2008-2015, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <wmstdio.h>
#include <lowlevel_drivers.h>
#include <wm_os.h>
#include <cli.h>
#include <stdlib.h>

#ifdef CONFIG_CPU_MW300

#define SZ_CACHE_PROF_DB	1000

static struct cache_counter_sample {
	uint32_t	miss_count;
	uint32_t	hit_count;
	uint32_t	start_tm;
	uint32_t	end_tm;
} samples[SZ_CACHE_PROF_DB];

static uint32_t cur_index;

__attribute__ ((section(".ram")))
static void _flush_cache(void)
{
	volatile uint32_t cnt = 0;

	FLASHC->FCCR.BF.CACHE_LINE_FLUSH = 1;

	while (cnt++ < 0x200000)
		if (FLASHC->FCCR.BF.CACHE_LINE_FLUSH == 0)
			break;
	if (cnt == 0x200000) {
		wmprintf("Error flushing the cache\r\n");
		return;
	}

	FLASHC->FCMCR.WORDVAL = 0x0;
	FLASHC->FCHCR.WORDVAL = 0x0;
}

__attribute__ ((section(".ram")))
int cacheprof_begin_reading(int flag)
{
	if (cur_index == SZ_CACHE_PROF_DB) {
		wmprintf("cacheprof - readings overflow\r\n");
		return -1;
	}

	samples[cur_index].start_tm = os_get_timestamp();

	if (flag == 1) {
		_flush_cache();
	}

	return 0;
}

__attribute__ ((section(".ram")))
int cacheprof_end_reading(void)
{
	if (cur_index == SZ_CACHE_PROF_DB) {
		return -1;
	}

	if (!samples[cur_index].start_tm || samples[cur_index].end_tm ||
			samples[cur_index].miss_count) {
		wmprintf("Non-matching cacheprof_end_reading\r\n");
		return -1;
	}

	samples[cur_index].end_tm = os_get_timestamp();
	samples[cur_index].hit_count = FLASHC->FCHCR.WORDVAL;
	samples[cur_index++].miss_count = FLASHC->FCMCR.WORDVAL;

	return 0;
}

__attribute__ ((section(".ram")))
void cacheprof_show_readings(void)
{
	uint32_t i;

	wmprintf("Current hit/miss counts:\r\n");
	wmprintf("Hits %u\r\n", FLASHC->FCHCR.WORDVAL);
	wmprintf("Misses %u\r\n\r\n", FLASHC->FCMCR.WORDVAL);
	wmprintf("Sample \t Duration \t Hits \t\t Misses\r\n");

	for (i = 0; i < cur_index; i++)
		wmprintf("%4d \t  %6d \t %6d \t %6d\r\n", i,
				samples[i].end_tm - samples[i].start_tm,
				samples[i].hit_count, samples[i].miss_count);
}

__attribute__ ((section(".ram")))
void cacheprof_clear_readings(void)
{
	memset(samples, 0, sizeof(samples));
	cur_index = 0;
}

__attribute__ ((section(".ram")))
static void flush_cache(int argc, char **argv)
{
	_flush_cache();
}

__attribute__ ((section(".ram")))
static void dump_cache_cnts(int argc, char **argv)
{
	cacheprof_show_readings();
}

__attribute__ ((section(".ram")))
static void clear_cache_cnts(int argc, char **argv)
{
	cacheprof_clear_readings();
}

__attribute__ ((section(".ram")))
static void begin_reading(int argc, char **argv)
{
	if (argc != 2) {
		wmprintf("%s <0/1>\r\n0 - Don't flush cache\r\n"
			 "1 - Flush cache\r\n",
				argv[0]);
		return;
	}

	cacheprof_begin_reading(atoi(argv[1]));
	return;
}

__attribute__ ((section(".ram")))
static void end_reading(int argc, char **argv)
{
	cacheprof_end_reading();
}

static struct cli_command cacheprof_cli_cmds[] = {
	{ "cache-flush", NULL, flush_cache },
	{ "cache-cnts", NULL, dump_cache_cnts },
	{ "cache-cnts-clear", NULL, clear_cache_cnts },
	{ "cache-begin-reading", "<0/1>" , begin_reading },
	{ "cache-end-reading", NULL, end_reading },
};

int cacheprof_cli_init(void)
{
	int ret;

	ret = cli_register_commands(&cacheprof_cli_cmds[0],
		sizeof(cacheprof_cli_cmds) / sizeof(struct cli_command));

	return ret;
}
#endif /* CONFIG_CPU_MW300 */
