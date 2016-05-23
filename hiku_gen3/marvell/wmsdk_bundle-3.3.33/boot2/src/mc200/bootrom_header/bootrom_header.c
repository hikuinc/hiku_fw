/*
 * Copyright (C) 2008-2013 Marvell International Ltd.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <crc.h>
#include <flash_layout.h>

#ifndef	JTAG_LOCK_PASSWORD
#define SECURITY_MODE		0x0
#define	JTAG_LOCK_PASSWORD	0x0
#else
#define SECURITY_MODE		0x00010204
#endif

/* Max boot2 size can be extended upto 23KiB for future use */
#define MAX_BOOT2_SIZE (4*1024)
/* Maximum bootrom header size */
#define MAX_HEADER (1024)

struct bootInfoHeader {
	uint32_t securityMode;
	uint32_t mainPasswd;
	union {
		struct {
			unsigned srcSection:8;
			unsigned clockSel:2;
			unsigned reserved:7;
			unsigned qspiPrescalerOff:1;
			unsigned qspiPrescaler:5;
			unsigned statusBit1:6;
			unsigned statusBit0:2;
			unsigned noWriteStatus:1;
		};
		uint32_t reg;
	} commonCfg0;
	uint32_t commonCfg1;
};

struct SectionHeader {
	uint32_t codeSig;
	uint32_t codeLen;
	uint32_t flashStartAddr;
	uint32_t sramStartAddr;
	uint32_t sramEntryAddr;
	union {
		struct {
			unsigned pNextSection:16;
			unsigned reserved:15;
			unsigned emptyCfg:1;
		};
		uint32_t reg;
	} bootCfg0;
	union {
		struct {
			unsigned memCfg0:3;
			unsigned memCfg1:3;
			unsigned qspiMode:3;
			unsigned crcMode:3;
			unsigned aesMode:3;
			unsigned reserved:17;
		};
		uint32_t reg;
	} bootCfg1;
	int32_t reserved;
};

static struct bootInfoHeader boot_h;
static struct SectionHeader section_h;
static struct stat fstat_buf;

struct readBuff {
	char buffer[MAX_BOOT2_SIZE];
	int val;
};

static struct readBuff read_t;
static char w_buffer[MAX_HEADER];

static int image_len(char *image)
{
	int fwfd;

#ifdef BUILD_MINGW
	fwfd = open(image, O_RDONLY | O_BINARY);
#else
	fwfd = open(image, O_RDONLY);
#endif
	if (fwfd < 0) {
		printf("Error: Cannot open %s for reading\n", image);
		exit(-1);
	}
	fstat(fwfd, &fstat_buf);
	close(fwfd);
	return fstat_buf.st_size;
}

static void read_file(char *image)
{
	int fwfd;

#ifdef BUILD_MINGW
	fwfd = open(image, O_RDONLY | O_BINARY);
#else
	fwfd = open(image, O_RDONLY);
#endif
	if (fwfd < 0) {
		printf("Error: Cannot open %s for reading\n", image);
		exit(-1);
	}

	read_t.val = read(fwfd, (void *)read_t.buffer, sizeof(read_t.buffer));
	if (read_t.val <= 0) {
		printf("Error: Cannot open %s for reading\n", image);
		exit(-1);
	}

	close(fwfd);
	return;
}

int main(int argc, char **argv)
{
	int fd;
	char *image, *bootfile;
	if (argc < 3) {
		printf("Usage: ./bootrom_header <input_file>"
			" <output_file>\n");
		exit(0);
	}

	image = argv[1];
	bootfile = argv[2];
	read_file(image);

	/* Empty flash should have oxff value */
	memset(w_buffer, 0xFF, sizeof(w_buffer));

	soft_crc32_init();

	/*
	 * Bootinfo Header
	 * To create secure BootROM header image, replace below lines with:
	 * boot_h.securityMode = 0x00010204;
	 * boot_h.mainPasswd = <password/key>;
	 */
	boot_h.securityMode = SECURITY_MODE;
	boot_h.mainPasswd = JTAG_LOCK_PASSWORD;
	/* Use flash copy as source medium */
	boot_h.commonCfg0.srcSection = 0x0;
	/* Keep system clock and qspi-prescalar as default */
	boot_h.commonCfg0.clockSel = 0x3;
	boot_h.commonCfg0.reserved = 0x7F;
	boot_h.commonCfg0.qspiPrescalerOff = 0x1;
	boot_h.commonCfg0.qspiPrescaler = 0x2;
	boot_h.commonCfg0.statusBit1 = 0x3F;
	boot_h.commonCfg0.statusBit0 = 0x3;
	boot_h.commonCfg0.noWriteStatus = 0x1;
	boot_h.commonCfg1 = 0xFFFFFFFF;

	/* Flash Section Header */
	section_h.codeSig = soft_crc32(read_t.buffer, read_t.val, 0);
	section_h.codeLen = image_len(image);
	section_h.flashStartAddr = FL_BOOT2_START + FL_BOOTROM_H_SIZE;
	/* As per boot2 linker script */
	section_h.sramStartAddr = 0x0015F000;
	/* As per boot2 linker script, entry point is fixed */
	section_h.sramEntryAddr = 0x0015F001;
	section_h.bootCfg0.reg = 0xFFFFFFFF;
	section_h.bootCfg1.reg = 0xFFFFFFFF;
	section_h.reserved = 0xFFFFFFFF;

#ifndef BUILD_MINGW
	fd = open(bootfile, O_CREAT | O_WRONLY, S_IRWXU | S_IRWXG | S_IRWXO);
#else
	/*
	 * Windows sys/stat.h does not have all the flags present in unix
	 * sys/stat header files.
	 *
	 * O_BINARY: Necessary flag without which extra '0x0D' is prepended
	 * to every '0x0A' in the write stream. This unnecessary addition
	 * results in boot failure on mingw compiled boot2 images.
	 */
	fd = open(bootfile, O_CREAT | O_WRONLY | O_BINARY, S_IRWXU);
#endif /* BUILD_MINGW */
	if (fd < 0) {
		printf("Error: Cannot open %s for write\n", bootfile);
		exit(-1);
	}

	memcpy(w_buffer, &boot_h, sizeof(boot_h));
	memcpy((w_buffer + sizeof(boot_h)), &section_h, sizeof(section_h));

	if (write(fd, w_buffer, sizeof(w_buffer)) < 0) {
		printf("File write failed\n");
		exit(-1);
	}

	close(fd);

	return 0;
}
