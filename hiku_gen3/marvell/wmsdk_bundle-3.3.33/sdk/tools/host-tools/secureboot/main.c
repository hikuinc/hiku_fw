/*
 * Copyright (C) 2015, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <secure_boot.h>
#include <soft_crc.h>

#include "utils.h"

#define MAKE_CLEANUP_FUNC make_cleanup(makefile, fp)

static void make_cleanup(void *makefile, FILE *fp)
{
	if (makefile)
		free(makefile);
	if (fp)
		fclose(fp);
}

int make_write_header(FILE *fp)
{
	int ret;
	char *line;

	line = "# Copyright (C) 2008-2015, Marvell International Ltd.\n"
		"# All Rights Reserved.\n\n"
		"# This is an auto-generated makefile.\n"
		"# Do not modify unless you know what you are doing.\n\n";

	ret = fwrite(line, 1, strlen(line), fp);
	sboot_assert((ret == strlen(line)), , -errno, "");
	return 0;
}

/* Generate a helper file for make build system */
int generate_makefile(char *infile, int boot2_add_header)
{
	FILE *fp = NULL;
	uint8_t encrypt_type, sign_type;
	uint32_t out_len;
	char suffix[MAX_SUFFIX] = {0}, outbuf[48];
	char *makefile;
	int ret = 0, ks_len;

	makefile = get_outdir_filename(infile, NULL);
	sboot_assert(makefile, , ERR_BAD_OUTFILE_NAME, "");

	fp = fopen(makefile, "w");
	sboot_assert(fp, MAKE_CLEANUP_FUNC, ERR_FILE_OPEN,
		"Could not open makefile %s for write: %s",
		makefile, strerror(errno));

	ret = make_write_header(fp);
	sboot_assert((ret >= 0), MAKE_CLEANUP_FUNC, ERR_FILE_WRITE,
		"Could not write header to makefile %s: %s",
		makefile, strerror(errno));

	/* Get boot2 suffix */
	ret = get_boot2_encrypt_type(&encrypt_type);
	sboot_assert((ret == 0), MAKE_CLEANUP_FUNC, ret, "");
	ret = get_boot2_sign_type(&sign_type);
	sboot_assert((ret == 0), MAKE_CLEANUP_FUNC, ret, "");

	/* Generate keystore only if not already created */
	if (tlv_store_validate(keystore, BOOT2_KEYSTORE, soft_crc32)
			!= TLV_SUCCESS)
		generate_keystore();
	ks_len = tlv_store_length(keystore);

	get_outfile_suffix(suffix, boot2_add_header, (ks_len >
				sizeof(boot2_ks_hdr)), encrypt_type, sign_type);
	snprintf(outbuf, sizeof(outbuf), "sec_boot2_type := %s\n", suffix);

	out_len = strlen(outbuf);
	ret = fwrite(outbuf, 1, out_len, fp);
	sboot_assert((ret == out_len), MAKE_CLEANUP_FUNC, ERR_FILE_WRITE,
		"Could not write data to makefile %s: %s",
		makefile, strerror(errno));

	/* Get MCUFW suffix */
	ret = get_fw_encrypt_type(&encrypt_type);
	sboot_assert((ret == 0), MAKE_CLEANUP_FUNC, ret, "");
	ret = get_fw_sign_type(&sign_type);
	sboot_assert((ret == 0), MAKE_CLEANUP_FUNC, ret, "");
	get_outfile_suffix(suffix, 0, 0, encrypt_type, sign_type);
	snprintf(outbuf, sizeof(outbuf), "sec_mcufw_type := %s\n", suffix);

	out_len = strlen(outbuf);
	ret = fwrite(outbuf, 1, out_len, fp);
	sboot_assert((ret == out_len), MAKE_CLEANUP_FUNC, ERR_FILE_WRITE,
		"Could not write data to makefile %s: %s",
		makefile, strerror(errno));

	fclose(fp);

	sboot_v("Created makefile %s", makefile);
	return 0;
}

int main(int argc, char **argv)
{
	int ret;
	struct arguments arguments;

	/* Parse arguments */
	memset(&arguments, 0, sizeof(arguments));
	ret = parse_opt(argc, argv, &arguments);
	if (ret < 0)
		return ret;

	ret = parse_config_file(arguments.config_file);
	if (ret < 0)
		return ret;

	if (arguments.boot2_file != NULL) {
		ret = generate_boot2(arguments.boot2_file, arguments.rawboot2);
		if (ret < 0)
			return ret;
	}

	if (arguments.fw_file != NULL) {
		ret = generate_fw(arguments.fw_file);
		if (ret < 0)
			return ret;
	}

	if (arguments.otp_file != NULL) {
		ret = generate_otpfile(arguments.otp_file);
		if (ret < 0)
			return ret;
	}

	if (arguments.ks_file != NULL) {
		ret = write_keystore_to_file(arguments.ks_file);
		if (ret < 0)
			return ret;
	}

	if (arguments.makefile != NULL) {
		ret = generate_makefile(arguments.makefile, arguments.rawboot2);
		if (ret < 0)
			return ret;
	}

	return 0;
}
