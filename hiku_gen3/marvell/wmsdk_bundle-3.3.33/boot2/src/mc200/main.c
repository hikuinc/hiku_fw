/*
 * Copyright (C) 2011-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

/* Standard includes. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <boot2.h>

/* Driver includes. */
#include <lowlevel_drivers.h>

void setup_stack(void) __attribute__((naked));
void initialize_bss(void) __attribute__((used));

unsigned long *nvram_addr = (unsigned long *)NVRAM_ADDR;

static void system_init(void)
{
	/* Initialize flash power domain */
	PMU_PowerOnVDDIO(PMU_VDDIO_FL);
}

int main(void)
{
	system_init();
	writel(SYS_INIT, nvram_addr);
	QSPI0_Init_CLK();

	writel(readel(nvram_addr) | FLASH_INIT, nvram_addr);
	dbg_printf("\r\nboot2: version %s\r\n", BOOT2_VERSION);
	dbg_printf("boot2: Hardware setup done...\r\n");
	boot2_main();
	/* boot2_main never returns */
	return 0;
}

void initialize_bss(void)
{
	/* Zero fill the bss segment. */
	memset(&_bss, 0, ((unsigned) &_ebss - (unsigned) &_bss));

	/* Call the application's entry point. */
	main();
}

void setup_stack(void)
{
	/* Setup stack pointer */
	asm("ldr r3, =_estack");
	asm("mov sp, r3");
	initialize_bss();
}
