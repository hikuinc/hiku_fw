/*
* Copyright (C) 2008-2015, Marvell International Ltd.
* All Rights Reserved.
*/

/* Entry Point */
ENTRY(Reset_IRQHandler)

/* Start address of main stack pointer
 * Note: Stack grows towards lower addresses.
*/
_estack = 0x20020000;    /* end of 128K SRAM1 */

/* Heap size in bytes */
_heap_size = (60 * 1024);

/* Size of BSS retained in PM3in bytes */
_pm3_bss1_size = (10624);

/* The offset at which text segment starts in the binary image, this should
 * not be modified: => sizeof(img_hdr) + (SEG_CNT * sizeof(seg_hdr))
 */
_text_offset = 0xc8;


/* This value is used by boot2 so it is always set to 0x20000000 */
_rom_data_start = 0x20000000;

/* Flash controller memory mapped start address */
_flashc_mem_start = 0x1f000000;

EXTERN(_rom_data)

MEMORY
{
	SRAM0 (rwx)  : ORIGIN = 0x00100000, LENGTH = 160K
	SRAM1 (rwx)  : ORIGIN = 0x20005800, LENGTH = 103K
	NVRAM (rw)   : ORIGIN = 0x480C0000, LENGTH = 4K
	FLASHC (rw)  : ORIGIN = 0x20020000, LENGTH = 32K
}

/* End of SRAM0 */
_e_sram0 = ORIGIN(SRAM0) + LENGTH(SRAM0);
/* Generate a link error if stack don't fit into SRAM.
 * Total stack size requirement will depend on the number of concurrent
 * threads in an application and the maximum stack required by each
 * thread.
 */
_min_stack_size = 0x800; /* required minimum amount of stack */

/* Start of main stack*/
_main_stack_start = ORIGIN(SRAM1) + LENGTH(SRAM1) - _min_stack_size;

SECTIONS
{
	/*
	 * Please note this section of RAM is retained in PM3
	 * It is 9KB in size.
	*/

        . = 0x20005800;
        .pm3_bss1 :
        {
		_pm3_bss1 = .;
                *libapp_framework.a: (.bss .bss.*)
                *libprovisioning.a: (.bss .bss.*)
                *libwlcmgr.a: (.bss .bss.*)
                *libdhcpd.a: (.bss .bss.*)
                *libpart.a: (.bss .bss.*)
                *libhealthmon.a: (.bss .bss.*)
                *libpwrmgr.a: (.bss .bss.*)
                *libnet.a: (.bss .bss.*)
                *libhttpd.a: (.bss .bss.*)
                *libled_indicator.a: (.bss .bss.*)
                *libwmtime.a: (.bss .bss.*)
                *libfreertos.a: (.bss .bss.*)
                . = ALIGN(4);
                _epm3_bss1 = .;
        } > SRAM1

	_actual_pm3_bss1_size = _epm3_bss1 - _pm3_bss1;
	 ASSERT(_pm3_bss1_size >= _actual_pm3_bss1_size, "PM3_BSS1 overflow.")

	. =0x00100000;
	.init :
	{
		init_s = .;
		. = ALIGN(256);
		KEEP(*(.isr_vector))
		KEEP(*(.text.Default_IRQHandler*))
		KEEP(*(.text.HardFault_IRQHandler*))
		*mw300_flash.o (.text .text.* .rodata .rodata.*)
		*mw300_qspi.o (.text .text.* .rodata .rodata.*)
		*mw300_pmu.o (.text .text.* .rodata .rodata.*)
		*mw300_clock.o (.text .text.* .rodata .rodata.*)
		*mdev_pm.o (.text .text.* .rodata .rodata.*)
		*mw300_gpio.o (.text .text.* .rodata .rodata.*)
		*mw300_pinmux.o (.text .text.* .rodata .rodata.*)
		*board.o (.text .text.* .rodata .rodata.*)
		*(.ram .ram.*)
		. = ALIGN(4);
	} > SRAM0

	.data :
	{
		. = . + _rom_data;
		_data = .;
		*(.data)
		*(.data.*)
		_edata = .;
	} > SRAM0

	/* Check for enough space for stack */
      .bss (NOLOAD):
	{
		_bss = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		_ebss = .;
	} > SRAM0
	/* Heap Section. */
	. = ALIGN(4);
	_heap_start = .;
	. = _e_sram0;
	_heap_end = .;
	_heap_2_start = .;
	_heap_2_end = .;

	_actual_heap_size = _heap_end - _heap_start + _heap_2_end - _heap_2_start;
	 ASSERT(_actual_heap_size >= _heap_size, "Insufficient space left for heap.")


	text_start = SIZEOF(.init) + SIZEOF(.data);
	.text (_flashc_mem_start + _text_offset + text_start):
	{
		. = ALIGN(4);

		*(.text.Reset_IRQHandler)
		*(.text .text.* .gnu.linkonce.t.*)
		*(.rodata .rodata.* .gnu.linkonce.r.*)

		. = ALIGN(4);
		_etext = .;
	}

	/*
 	* This memory section is placed in that part of SRAM1
 	* which will not be retained when MCU enters PM3.
 	* This section should hold data elements which
 	* get populated every time before use.
 	*/
	.pm3_bss  0x20008000 (NOLOAD):
	{
		_pm3_bss = .;
		*(.pm3_bss)
		*(.pm3_bss.*)
		*(COMMON)
		_epm3_bss = .;
	} > SRAM1
	. = _epm3_bss;
	/* Check for enough space for stack */
        ._main_stack _main_stack_start :
        {
                . = ALIGN(4);
                . = . + _min_stack_size;
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
