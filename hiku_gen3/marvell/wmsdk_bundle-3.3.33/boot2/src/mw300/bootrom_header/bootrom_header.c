/*
 * Copyright (C) 2008-2014 Marvell International Ltd.
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
#include <flash_layout.h>
#include <bootrom_header.h>
#include "crc.h"

#define BOOT2_START 0x0015C000
#define BOOT2_ENTRY ((BOOT2_START) | 0x1)
#define ALIGN_SIZE 16

static es_t es;
static hdr_t h;
static uint8_t aes_mic[16];

struct readBuff {
	char *buffer;
	int val;
};
static struct readBuff read_t;

static void die_perror(char *msg)
{
	perror(msg);
	exit(1);
}

static int image_len(char *image)
{
	int fwfd;
	struct stat fstat_buf;

#ifdef BUILD_MINGW
	fwfd = open(image, O_RDONLY | O_BINARY);
#else
	fwfd = open(image, O_RDONLY);
#endif
	if (fwfd < 0) {
		printf("Error: Cannot open %s for reading\n", image);
		exit(-1);
	}

	if (fstat(fwfd, &fstat_buf) < 0)
		die_perror("fstat failed");

	close(fwfd);
	return fstat_buf.st_size;
}

static void read_file(char *image, int len)
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

	read_t.val = read(fwfd, (void *)read_t.buffer, len);
	if (read_t.val != len)
		die_perror("read failed");

	close(fwfd);
	return;
}

int main(int argc, char **argv)
{
	int fd;
	uint32_t codeSig;
	char *image, *bootfile;

	if (argc < 3) {
		printf("Usage: ./bootrom_header <input_file>"
						" <output_file>\n");
		exit(0);
	}

	image = argv[1];
	bootfile = argv[2];
	int file_len = image_len(image);
	read_t.buffer = (char *) malloc(file_len + ALIGN_SIZE);
	if (!read_t.buffer)
		die_perror("malloc failed");
	read_file(image, file_len);

	/* Reserved field in headers should be all 1's */
	memset(&h, 0xFF, sizeof(h));

	soft_crc16_init();

	/* Enable DMA when loading code to SRAM */
	h.bh.commonCfg0.dmaEn = 0x1;
	/* Use SFLL (192MHz) for system clock */
	h.bh.commonCfg0.clockSel = 0x0;
	h.bh.commonCfg0.flashintfPrescalarOff = 0x0;
	/* Use prescaler to use 24MHz (192/8) for QSPI clock */
	h.bh.commonCfg0.flashintfPrescalar = 0b01000;
	/* Write magic code 'MRVL' */
	h.bh.magicCode = 0x4D52564C;
	/* As per boot2 linker script, entry point is fixed */
	h.bh.entryAddr = BOOT2_ENTRY;
	/* Following are just default values, un-used ones */
	h.bh.sfllCfg = 0xFE50787F;
	h.bh.flashcCfg0 = 0xFF053FF5;
	h.bh.flashcCfg1 = 0x07FFCC8C;
	h.bh.flashcCfg2 = 0x0;
	h.bh.flashcCfg3 = 0x0;
	/* Add CRC16 field */
	h.bh.CRCCheck = (uint16_t) soft_crc16(&h.bh,
				sizeof(h.bh) - sizeof(h.bh.CRCCheck));

	uint32_t aligned_length = read_t.val;
	/* Align length to 16 byte boundary for AES-CCM */
	aligned_length = (read_t.val + sizeof(codeSig) + ALIGN_SIZE - 1)
						& (~(ALIGN_SIZE - 1));
	aligned_length -= sizeof(codeSig);
	memset(read_t.buffer + read_t.val, 0xff, aligned_length - read_t.val);

	/* Flash Section Header */
	codeSig = (uint16_t) soft_crc16(read_t.buffer,
						aligned_length);
	h.sh.codeLen = aligned_length;
	/* As per boot2 linker script */
	h.sh.destStartAddr = BOOT2_START;
	h.sh.bootCfg0.reg = 0xFFFFFFFF;
	h.sh.bootCfg1.reg = 0xFFFFFE3E;
	h.sh.CRCCheck =  (uint16_t) soft_crc16(&h.sh,
				sizeof(h.sh) - sizeof(h.sh.CRCCheck));

#ifndef BUILD_MINGW
	fd = open(bootfile, O_CREAT | O_WRONLY, S_IRWXU);
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

	uint32_t hdr_len = sizeof(h);
	uint32_t adjust_len = aligned_length + hdr_len + sizeof(codeSig);

	read_t.buffer = (char *) realloc(read_t.buffer, adjust_len);
	if (!read_t.buffer)
		die_perror("realloc failed");

	memmove(read_t.buffer + hdr_len, read_t.buffer, aligned_length);
	memcpy(read_t.buffer, &h.bh, hdr_len);
	memcpy(read_t.buffer + hdr_len + aligned_length, &codeSig,
							sizeof(codeSig));
	if (write(fd, &es, sizeof(es)) < 0)
		perror("write failed");
	if (write(fd, read_t.buffer, adjust_len) < 0)
		perror("write failed");
	if (write(fd, aes_mic, sizeof(aes_mic)) < 0)
		perror("write failed");

	close(fd);

	free(read_t.buffer);

	return 0;
}
