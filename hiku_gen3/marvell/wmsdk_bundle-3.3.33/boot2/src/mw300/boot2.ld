/*
 * Copyright (C) 2015, Marvell International Ltd.
 * All Rights Reserved.
 */

/* Entry point */
ENTRY(setup_stack)

/* Highest address of the user mode stack */
_estack = 0x20020000;    /* end of 128K RAM1 */

/* Generate a link error if heap and stack don't fit into RAM */
_min_stack_size = 0x1000; /* required amount of stack */
_min_heap_size = 0x3000; /* required amount of heap */

/* Generate a link error if keystore area do not fit in RAM */
_keystore_size = 0x1000;

/* ROM data section */
_rom_data_start = 0x20000000;
_rom_data_size = 64;

MEMORY
{
	SRAM0 (rwx)  : ORIGIN = 0x0015C000, LENGTH = 16K
	SRAM1 (rwx)  : ORIGIN = 0x2001B000, LENGTH = 20K
}

SECTIONS
{
	.text :
	{
		*(.text.setup_stack)
		*(.text .text.* .gnu.linkonce.t.*)
		*(.rodata .rodata.* .gnu.linkonce.r.*)
		. = ALIGN(4);
		_etext = .;
	} > SRAM0

	.data :
	{
		_data = .;
		*(.data)
		*(.data.*)
		_edata = .;
	} > SRAM0

	/* Make sure this is the last loadable section in SRAM0, because this
	 * will be attached at the end of boot2 as binary blob, if secure boot
	 * scheme is enabled.
	 */
	.keystore :
	{
		/* This alignment is required for AES-CCM encryption */
		. += 4;
		. = ALIGN(0x10);
		_keystore_start = .;
		. = . + _keystore_size;
		_keystore_end = .;
	} > SRAM0

	.bss :
	{
		. = ALIGN(4);
		_bss = .;
		*(.bss)
		*(.bss.*)
		. = ALIGN(4);
		_ebss = .;
	} > SRAM1

	/* Check for enough space for heap */
	.heap :
	{
		. = ALIGN(4);
		_heap_start = .;
		. = . + _min_heap_size;
		. = ALIGN(4);
		_heap_end = .;
	} > SRAM1

	/* Check for enough space for stack */
	.main_stack :
	{
		. = ALIGN(4);
		. = . + _min_stack_size;
		. = ALIGN(4);
	} > SRAM1
}
