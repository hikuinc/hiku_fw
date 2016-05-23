/*
 * Copyright (C) 2015, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <ctype.h>
#include <stdint.h>
#include <secure_boot.h>

#include "utils.h"

#define PATH_SEP_UNIX '/'
#define PATH_SEP_WIN '\\'
#define PATH_SEP_STR "/"

/* Global Declarations */
int verbose;
char *outdir;

void print_usage(char *prog)
{
	prog = basename(prog);
	sboot_i("%s can be used in following ways:", prog);
	printf("%s -b BOOT2FILE.BIN -o OTPOUT.H -c CONFIG.CSV [-k KEYSTORE.BIN]"
		" [-d OUTPUT_DIR] [-v]\n", prog);
	printf("%s -m MCU_FIRMWARE.BIN -c CONFIG.CSV [-d OUTPUT_DIR] [-v]\n",
		prog);
	printf("%s -b BOOT2FILE.BIN -o OTPOUT.H -m MCU_FIRMWARE.BIN"
		" -c CONFIG.CSV [-k KEYSTORE.BIN] [-d OUTPUT_DIR] [-v]\n",
		prog);
	printf("%s -r RAWBOOT2.BIN -o OTPOUT.H -c CONFIG.CSV [-k KEYSTORE.BIN]"
		" [-d OUTPUT_DIR] [-v]\n", prog);
	printf("       where optional -v argument produces verbose output\n");
	printf("       Optional -d argument specifies output directory\n");
	printf("       If -d argument is not given, files are created in the"
		" same folder as input files\n");
	printf("       Optional -k argument produces stand-alone keystore"
		" binary\n");
	printf("       RAWBOOT2.BIN is a 2nd-stage bootloader/firmware"
		" without bootrom header\n");
	printf("       Please refer to WMSDK Secure Boot Application Note for"
		" details\n");
	printf("%s -m MAKEFILE -c CONFIG.CSV [-v]\n", prog);
}

#ifdef TEST_BUILD
enum test_num_t test_num;
#endif

int parse_opt(int argc, char **argv, struct arguments *arguments)
{
	int key;

#ifdef TEST_BUILD
	while ((key = getopt(argc, argv, "b:m:r:c:o:k:M:d:t:vh")) != -1) {
#else
	while ((key = getopt(argc, argv, "b:m:r:c:o:k:M:d:vh")) != -1) {
#endif
		switch (key) {
		case 'b':
			arguments->boot2_file = optarg;
			break;
		case 'm':
			arguments->fw_file = optarg;
			break;
		case 'r':
			arguments->boot2_file = optarg;
			arguments->rawboot2 = true;
			break;
		case 'c':
			arguments->config_file = optarg;
			break;
		case 'o':
			arguments->otp_file = optarg;
			break;
		case 'k':
			arguments->ks_file = optarg;
			break;
		case 'M':
			arguments->makefile = optarg;
			break;
		case 'd':
			outdir = optarg;
			break;
#ifdef TEST_BUILD
		case 't':
			sscanf(optarg, "%x\n", &test_num);
			break;
#endif
		case 'v':
			verbose = 1;
			break;
		case 'h':
			print_usage(argv[0]);
			return ERR_INVALID_ARGS;
		}
	}

	if (arguments->config_file)
		return 0;

	sboot_e("Invalid arguments");
	print_usage(argv[0]);
	return ERR_INVALID_ARGS;
}

int file_len(char *file)
{
	FILE *fp;
	int ret;

	errno = 0;
	fp = fopen(file, "rb");
	sboot_assert(fp, , -errno, "");

	ret = fseek(fp, 0L, SEEK_END);
	if (ret < 0)
		goto done;

	ret = ftell(fp);

done:
	fclose(fp);
	return ret;
}

int file_read(char *file, uint8_t **buf)
{
	FILE *fp;
	int ret, len;

	errno = 0;
	fp = fopen(file, "rb");
	sboot_assert(fp, , -errno, "");

	ret = fseek(fp, 0L, SEEK_END);
	if (ret < 0)
		goto done;

	ret = ftell(fp);
	if (ret < 0)
		goto done;

	len = ret;
	ret = fseek(fp, 0L, SEEK_SET);
	if (ret < 0)
		goto done;

	*buf = malloc(len);
	if (*buf == NULL) {
		errno = -ENOMEM;
		ret = -1;
		goto done;
	}

	ret = fread(*buf, 1, len, fp);
	if (len != ret) {
		free(*buf);
		*buf = NULL;
		ret = -1;
	}

done:
	fclose(fp);
	return ret;
}

/* Get output file name in output directory. It essentially extracts the
 * basename of the infile and prefixes the outdir value to it. If outdir
 * is NULL, it simply returns a copy of infile. */
char *get_outdir_filename(char *infile, char *suffix)
{
	char *outfile = NULL, *basename = NULL, *extn = NULL;
	int outlen = 0;

	extn = strrchr(infile, '.');

	if (outdir != NULL) {
#if defined(_WIN32) || defined(_WIN64)
		/* Convert Windows style path separator to Unix style */
		basename = infile;
		while (basename && *basename != '\0') {
			if (*basename == PATH_SEP_WIN)
				*basename = PATH_SEP_UNIX;
			basename++;
		}
		basename = outdir;
		while (basename && *basename != '\0') {
			if (*basename == PATH_SEP_WIN)
				*basename = PATH_SEP_UNIX;
			basename++;
		}
#endif
		basename = strrchr(infile, PATH_SEP_UNIX);
		if (basename != NULL && basename[0] == PATH_SEP_UNIX)
			basename++;
		if (basename != NULL)
			infile = basename;
		outlen = strlen(outdir) + 1 + strlen(infile) + 1;
	} else
		outlen = strlen(infile) + 1;

	if (suffix != NULL)
		outlen += strlen(suffix);

	outfile = malloc(outlen);
	sboot_assert(outfile, , NULL, "Failed to allocate memory for"
			" output file name");

	outfile[0] = '\0';
	if (outdir != NULL) {
		strcat(outfile, outdir);
		if (outdir[strlen(outdir) - 1] != PATH_SEP_UNIX)
			strcat(outfile, PATH_SEP_STR);
	}
	if (extn != NULL)
		strncat(outfile, infile, extn - infile);
	else
		strcat(outfile, infile);
	if (suffix != NULL)
		strcat(outfile, suffix);
	if (extn != NULL)
		strcat(outfile, extn);

	return outfile;
}

/* Fills the input string with appropriate suffix characters */
void get_outfile_suffix(char *suffix, uint8_t add_header, uint8_t keystore,
			uint8_t encrypt, uint8_t sign)
{
	int idx = 0;

	if (add_header || keystore || (encrypt != NO_ENCRYPT) ||
			(sign != NO_SIGN))
		suffix[idx++] = '.';

	if (add_header)
		suffix[idx++] = 'h';

	if (keystore)
		suffix[idx++] = 'k';

	if (encrypt != NO_ENCRYPT)
		suffix[idx++] = 'e';

	if (sign != NO_SIGN)
		suffix[idx++] = 's';

	suffix[idx] = '\0';
	return;
}

/* Generate output name by inserting proper suffixes in input name and
 * also prefixing otudir name if available */
char *get_outfile_name(char *infile, uint8_t add_header, uint8_t keystore,
			uint8_t encrypt, uint8_t sign)
{
	char suffix[MAX_SUFFIX] = {0};

	get_outfile_suffix(suffix, add_header, keystore, encrypt, sign);
	return get_outdir_filename(infile, suffix);
}

/* Convert a decimal string to integer or long */
int dec2bin(const char *ibuf, void *obuf, unsigned max_olen)
{
	long val;
	char *endptr = NULL;

	errno = 0;
	val = strtol(ibuf, &endptr, 10);

	/* Check for various possible errors */
	if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
			|| (errno != 0 && val == 0))
		return 0;

	if (*endptr != '\0')
		return 0;
	if (max_olen == sizeof(long))
		*(long *)obuf = val;
	else if (max_olen == sizeof(int))
		*(int *)obuf = (int)val;
	else
		return 0;

	return max_olen;
}

/* Convert a binary string to binary data */
int binstr2bin(const char *ibuf, uint8_t *obuf, unsigned max_olen)
{
	/* TODO: Implement this like dec2bin */
	return 0;
}

/* Convert a hexadecimal string to binary data */
uint32_t hex2bin(const char *ibuf, uint8_t *obuf, unsigned max_olen)
{
	unsigned int i;		/* loop iteration variable */
	unsigned int j = 0;	/* current character */
	unsigned int by = 0;	/* byte value for conversion */
	unsigned char ch;	/* current character */
	unsigned int len = strlen((char *)ibuf);

	/* process the list of characters */
	for (i = 0; (len & 1) ? (i < len + 1) : (i < len); i++) {
		if (i == (2 * max_olen)) {
			return j + 1;
		}
		if ((len & 1) && i == 0)
			ch = '0';
		else
			/* get next uppercase char */
			ch = toupper((int) *ibuf++);

		/* do the conversion */
		if (ch >= '0' && ch <= '9')
			by = (by << 4) + ch - '0';
		else if (ch >= 'A' && ch <= 'F')
			by = (by << 4) + ch - 'A' + 10;
		else {		/* error if not hexadecimal */
			return 0;
		}

		/* store a byte for each pair of hexadecimal digits */
		if (i & 1) {
			j = ((i + 1) / 2) - 1;
			obuf[j] = by & 0xff;
		}
	}
	return j + 1;
}

/* Convert binary data to printable hex string */
char *bin2hex(const uint8_t *src, char *dest, unsigned int src_len,
		unsigned int dest_len)
{
	int i;
	char *ret = dest;

	for (i = 0; i < src_len; i++) {
		if (snprintf(dest, dest_len, "%02X", src[i]) >= dest_len) {
			sboot_w("%s: Destination full. "
				"Truncating to avoid overflow.", __func__);
			return ret;
		}
		dest_len -= 2;
		dest += 2;
	}

	return ret;
}

/* Fill buffer with random data */
int PlatformRandomBytes(void *inBuffer, size_t inByteCount)
{
	int fd;
	fd = open("/dev/urandom", O_RDONLY);
	if (fd) {
		if (read(fd, inBuffer, inByteCount) < inByteCount)
			return -1;
		close(fd);
		return 0;
	}
	return -1;
}

#ifdef TEST_BUILD
/* Functions to generate negative test images for secure boot validation */
#include <time.h>
#define IS_TEST_BIT_SET(test) (test_num & (test))

void corrupt_keystore(uint8_t *keystore)
{
	srand(time(NULL));

	if (IS_TEST_BIT_SET(TEST_BAD_KS_MAGIC))
		((boot2_ks_hdr *)keystore)->magic = rand();
	if (IS_TEST_BIT_SET(TEST_BAD_KS_LEN))
		((boot2_ks_hdr *)keystore)->len = rand() % SB_KEYSTORE_SIZE;
	if (IS_TEST_BIT_SET(TEST_BAD_KS_CRC))
		((boot2_ks_hdr *)keystore)->crc = rand();
}

void corrupt_sec_hdr(uint8_t *sec_hdr)
{
	srand(time(NULL));

	if (IS_TEST_BIT_SET(TEST_BAD_FW_SHMAGIC))
		((img_sec_hdr *)sec_hdr)->magic = rand();
	if (IS_TEST_BIT_SET(TEST_BAD_FW_SHLEN))
		((img_sec_hdr *)sec_hdr)->len = rand() % SB_SEC_IMG_HDR_SIZE;
	if (IS_TEST_BIT_SET(TEST_BAD_FW_SHCRC))
		((img_sec_hdr *)sec_hdr)->crc = rand();
}

void corrupt_sec_hdr_keys(uint8_t *sec_hdr)
{
	uint32_t bad_val;

	srand(time(NULL));

	bad_val = rand();
	if (IS_TEST_BIT_SET(TEST_BAD_FW_IMGLEN))
		tlv_store_add(sec_hdr, KEY_FW_IMAGE_LEN, sizeof(bad_val),
				(uint8_t *)&bad_val);
	bad_val = rand();
	if (IS_TEST_BIT_SET(TEST_BAD_FW_NONCE))
		tlv_store_add(sec_hdr, KEY_FW_NONCE, sizeof(bad_val),
				(uint8_t *)&bad_val);
	bad_val = rand();
	if (IS_TEST_BIT_SET(TEST_BAD_FW_MIC))
		tlv_store_add(sec_hdr, KEY_FW_MIC, sizeof(bad_val),
				(uint8_t *)&bad_val);
	bad_val = rand();
	if (IS_TEST_BIT_SET(TEST_BAD_FW_SIGN))
		tlv_store_add(sec_hdr, KEY_FW_SIGNATURE, sizeof(bad_val),
				(uint8_t *)&bad_val);
}
#endif
