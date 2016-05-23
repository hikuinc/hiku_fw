/*
 * Copyright (C) 2008-2016, Marvell International Ltd.
 * All Rights Reserved.
 */

/* Entry Point */
ENTRY(Reset_IRQHandler)

/* Start address of main stack pointer */
_estack = 0x20008000;    /* end of 32K  SRAM1 */

/* Heap size in bytes */
_heap_size = (20 * 1024);

/* Size of .data section for libs in ROM, this must start at ORIGIN(SRAM1) */
EXTERN(_rom_data)

MEMORY
{
/*In PM3 only 192K SRAM is alive */
/* 160K of SRAM 0*/
/* 32K of SRAM 1 */
/* The bootrom code uses 22KB of SRAM1 after
 * exit from PM3 so only 10KB is available for SDK
 * to store data in SRAM1
*/
	SRAM0 (rwx)  : ORIGIN = 0x00100000, LENGTH = 156K
	SRAM1 (rwx)  : ORIGIN = 0x20002800, LENGTH = 10K
	NVRAM (rw)   : ORIGIN = 0x480C0000, LENGTH = 4K
}

/* Generate a link error if stack don't fit into SRAM */
 _main_stack_size = 0x800; /* required minimum amount of stack */
_main_stack_start = ORIGIN(SRAM1) + LENGTH(SRAM1) - _main_stack_size;

/* Flash controller memory mapped start address */
_flashc_mem_start = 0x1f000000;

SECTIONS
{
	.text :
	{
		. = ALIGN(256);
		KEEP(*(.isr_vector))
		. = ALIGN(4);

		*(.text.Reset_IRQHandler)
		*(.text .text.* .gnu.linkonce.t.*)
		*(.rodata .rodata.* .gnu.linkonce.r.*)

		. = ALIGN(4);
		_etext = .;
	} > SRAM0

	.heapdata (NOLOAD):
	{
		. = ALIGN(4);
		_heap_start = .;
		. += _heap_size;
		. = ALIGN(4);
		_heap_end = .;
	} > SRAM0

	.data :
	{
		_data = .;
		*(.data)
		*(.data.*)
		_edata = .;
	} > SRAM0

	.rom_data (NOLOAD) :
	{
		_rom_data_start = .;
		. = . + _rom_data;
	} > SRAM1

	ASSERT( _rom_data_start == ORIGIN(SRAM1), "Invalid start address of .rom_data")

	.bss (NOLOAD):
	{
		_bss = .;
		*(.bss)
		*(.bss.*)
                _pm3_bss = .;
                *(.pm3_bss)
                *(.pm3_bss.*)
                _epm3_bss = .;
                _pm3_bss1 = .;
                *(.pm3_bss1)
                *(.pm3_bss1.*)
                _epm3_bss1 = .;
		*(COMMON)
		_ebss = .;
	} > SRAM1

        /* Add the free space in SRAM1 to our heap */
	_heap_2_start = .;
	. = _main_stack_start;
	_heap_2_end = .;

        /* The main stack */
        ._main_stack _main_stack_start :
        {
                . = ALIGN(4);
                . = . + _main_stack_size;
                . = ALIGN(4);
        }


	.nvram (NOLOAD):
	{
		/* BootROM uses first few bytes of retention ram */
		_nvram_start = .;
		. = . + 64;
		. = ALIGN(4);
		/* Zero initialized on bootup */
		_nvram_begin = .;
		*(.nvram)
		*(.nvram.*)
		_nvram_end = .;
		/* Un-initialized nvram section */
		. = ALIGN(4);
		*(.nvram_uninit)
		*(.nvram_uninit.*)
	} > NVRAM

	/DISCARD/ :
	{
		*(.ARM.exidx* .gnu.linkonce.armexidx.*)
	}
}
