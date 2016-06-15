/*
 * Copyright (C) 2008-2014, Marvell International Ltd.
 * All Rights Reserved.
 */

/* Entry Point */
ENTRY(Reset_IRQHandler)

/* Start address of main stack pointer
 * Note: Stack grows towards lower addresses.
 */
_estack = 0x20020000;    /* end of 128K SRAM1 */

/* Minimum required heap size in bytes */
_heap_size = (72 * 1024);

/* The offset at which text segment starts in the binary image, this should
 * not be modified: => sizeof(img_hdr) + (SEG_CNT * sizeof(seg_hdr))
 */
_text_offset = 0xc8;

/* Flash controller memory mapped start address */
_flashc_mem_start = 0x1f000000;

/* Size of .data section for libs in ROM, this must start at ORIGIN(SRAM1) */
EXTERN(_rom_data)

MEMORY
{
	SRAM0 (rwx)  : ORIGIN = 0x00100000, LENGTH = 384K
	SRAM1 (rwx)  : ORIGIN = 0x20000000, LENGTH = 128K
	NVRAM (rw)   : ORIGIN = 0x480C0000, LENGTH = 4K
	FLASHC (rw)  : ORIGIN = 0x20020000, LENGTH = 32K
}

/* The size of the main stack that is used for interrupt processing. */
_main_stack_size = 0x800;
/* Start of main stack*/
_main_stack_start = ORIGIN(SRAM1) + LENGTH(SRAM1) - _main_stack_size;

/* Keystore size in RAM, SB_KEYSTORE_SIZE from secure_boot.h  */
_keystore_size = 0x1000;
/* Start of keystore */
_keystore_start = ORIGIN(SRAM0) + LENGTH(SRAM0) - _keystore_size;

SECTIONS
{
	.init :
	{
		. = ALIGN(256);
		KEEP(*(.isr_vector))
		KEEP(*(.text.Default_IRQHandler*))
		KEEP(*(.text.HardFault_IRQHandler*))
		*mw300_flash.o (.text .text.* .rodata .rodata.*)
		*mw300_qspi.o (.text .text.* .rodata .rodata.*)
		*mw300_pmu.o (.text .text.* .rodata .rodata.*)
		*mdev_pm.o (.text .text.* .rodata .rodata.*)
		*mw300_gpt.o (.text .text.* .rodata .rodata.*)
		*mw300_uart.o (.text .text.* .rodata .rodata.*)
		*mw300_clock.o (.text .text.* .rodata .rodata.*)
		*int_time.o (.text .text.* .rodata .rodata.*)
		*int_50ms_time.o (.text .text.* .rodata .rodata.*)
		*app_time.o (.text .text.* .rodata .rodata.*)
		*int_sci0.o (.text .text.* .rodata .rodata.*)
		*calculate_a_crc-16_in_C.o (.text .text.* .rodata .rodata.*)
		*os_eeprm.o (.text .text.* .rodata .rodata.*)
		*(.ram .ram.*)
		. = ALIGN(4);
	} > SRAM0

   .bss1 (NOLOAD):
   {
      _bss1 = .;
      *liblwip.a:(.bss .bss.*)
      . = ALIGN(4);
      _ebss1 = .;
   } > SRAM0


	/* Heap Section. */
	. = ALIGN(4);
	_heap_start = .;
	. = _keystore_start;
	_heap_end = .;

	.keystore _keystore_start :
	{
		. = . + _keystore_size;
	}

	.text (_flashc_mem_start + _text_offset + SIZEOF(.init)):
	{
		. = ALIGN(4);

		*(.text.Reset_IRQHandler)
		*(.text .text.* .gnu.linkonce.t.*)
		*(.rodata .rodata.* .gnu.linkonce.r.*)

		. = ALIGN(4);
		_etext = .;
	}

	.rom_data (NOLOAD) :
	{
		_rom_data_start = .;
		. = . + _rom_data;
	} > SRAM1

	ASSERT( _rom_data_start == ORIGIN(SRAM1), "Invalid start address of .rom_data")

	.data :
	{
		_data = .;
		*(.data)
		*(.data.*)
		_edata = .;
	} > SRAM1

   .encdf_data :
   {
		_start_of_encdf_data = .;
		*(.encdf_data)
		*(.encdf_data.*)
		_end_of_encdf_data = .;
	} > SRAM1

   /* .encdf_data must fit into one flash block (4K) including the 2-byte CRC-16 */
   _size_of_encdf_data = _end_of_encdf_data - _start_of_encdf_data;
   ASSERT( _size_of_encdf_data <= 4094, ".encdf_data cannot exceed 4094 bytes")

	.bss (NOLOAD):
	{
		_bss = .;
		*(.bss)
		*(.bss.*)
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

	_actual_heap_size = _heap_end - _heap_start + _heap_2_end - _heap_2_start;
	 ASSERT(_actual_heap_size >= _heap_size, "Insufficient space left for heap.")

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
