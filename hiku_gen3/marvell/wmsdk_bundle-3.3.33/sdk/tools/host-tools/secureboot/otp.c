/*
 * Copyright (C) 2015, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <secure_boot.h>
#include <soft_crc.h>
#include "security.h"
#include "utils.h"

int otp_write_header(FILE *otp)
{
	int ret;
	char *line;

	line = "/*\n"
		" * Copyright (C) 2008-2015, Marvell International Ltd.\n"
		" * All Rights Reserved.\n\n"
		" * This is an auto-generated header file.\n"
		" * Do not modify unless you know what you are doing.\n"
		" */\n\n"
		"#ifndef __SECURE_CONFIG_H__\n"
		"#define __SECURE_CONFIG_H__\n";
	ret = fwrite(line, 1, strlen(line), otp);
	sboot_assert((ret == strlen(line)), , -errno, "");
	return 0;
}

/* Macro to write a #define */
#define WRITE_FLAG_DEF(line, flag) \
	do { \
		ret = fwrite(line, 1, strlen(line), otp); \
		sboot_assert((ret == strlen(line)), , -errno, ""); \
		line = (flag) ? "1\n" : "0\n"; \
		ret = fwrite(line, 1, strlen(line), otp); \
		sboot_assert((ret == strlen(line)), , -errno, ""); \
	} while (0);

#define WRITE_STR_DEF(line, data, len) \
	do { \
		ret = fwrite(line, 1, strlen(line), otp); \
		sboot_assert((ret == strlen(line)), , -errno, ""); \
		\
		if (len != 0 && data != NULL) \
			bin2hex(data, hex, len, sizeof(hex)); \
		ret = fwrite(hex, 1, strlen(hex), otp); \
		sboot_assert((ret == strlen(hex)), , -errno, ""); \
		\
		line = "\"\n"; \
		ret = fwrite(line, 1, strlen(line), otp); \
		sboot_assert((ret == strlen(line)), , -errno, ""); \
	} while (0);

int otp_write_boot_config(FILE *otp, uint8_t boot_config)
{
	int ret;
	char *line;

	line = "\n/* Enable JTAG after boot, UART/USB without password\n"
		" * Set this to 0 for disabling JTAG.\n"
		" */\n#define JTAG_STATUS ";
	WRITE_FLAG_DEF(line, !(boot_config & OTP_JTAG_DISABLE));

	line = "\n/* Enable UART/USB boot\n"
		" * Set this to 0 for disabling UART/USB boot modes.\n"
		" */\n#define UART_USB_BOOT ";
	WRITE_FLAG_DEF(line, !(boot_config & OTP_USB_UART_BOOT_DISABLE));

	line = "\n/* Enable encrypted image boot\n"
		" * Set this to 0 for disabling encrypted boot mode.\n"
		" */\n#define ENCRYPTED_BOOT ";
	WRITE_FLAG_DEF(line, boot_config & OTP_ENCRYPTED_BOOT);

	line = "\n/* Enable signed image boot\n"
		" * Set this to 0 for disabling signed boot mode.\n"
		" */\n#define SIGNED_BOOT ";
	WRITE_FLAG_DEF(line, boot_config & OTP_SIGNED_BOOT);

	return 0;
}

int otp_write_decrypt_key(FILE *otp, const uint8_t *key, uint32_t len)
{
	char hex[128] = {0};
	int ret;
	char *line;

	line = "\n/* AES-CCM 256 bit key, 64 hex characters */\n"
		"#define AES_CCM256_KEY \\\n\t\"";
	WRITE_STR_DEF(line, key, len);
	return 0;
}

int otp_write_public_key_hash(FILE *otp, const uint8_t *hash, uint32_t len)
{
	char hex[128] = {0};
	int ret;
	char *line;

	line = "\n/* SHA256 checksum of RSA Public Key, 64 hex characters */\n"
		"#define RSA_PUB_KEY_SHA256 \\\n\t\"";
	WRITE_STR_DEF(line, hash, len);
	return 0;
}

int otp_write_user_data(FILE *otp, const uint8_t *udata, uint32_t len)
{
	char hex[2 * MAX_OTP_USER_DATA] = {0};
	int ret;
	char *line;

	if (len > MAX_OTP_USER_DATA)
		return ERR_INVALID_ARGS;

	line = "\n/* Enable writing user specific data to OTP memory\n"
		" * Set this to 1 for enabling user data write.\n"
		" * Note: Maximum size for user data can be up-to 252 bytes.\n"
		" */\n#define USER_DATA ";
	WRITE_FLAG_DEF(line, (len != 0));

	line = "\n/* User specific additional data in OTP memory */\n"
		"#define OTP_USER_DATA \\\n\t\"";
	WRITE_STR_DEF(line, udata, len);
	return 0;
}

int otp_close_file(FILE *otp)
{
	int ret;
	char *line;

	line = "\n#endif /* __SECURE_CONFIG_H__ */\n";
	ret = fwrite(line, 1, strlen(line), otp);
	sboot_assert((ret == strlen(line)), fclose(otp), -errno, "");

	fclose(otp);
	return 0;
}

#define OTP_CLEANUP_FUNC otp_cleanup(hash, otpfile, otp)

static void otp_cleanup(void *hash, void *otpfile, FILE *otp)
{
	if (hash)
		free(hash);
	if (otpfile)
		free(otpfile);
	if (otp)
		otp_close_file(otp);
}

int generate_otpfile(char *infile)
{
	FILE *otp = NULL;
	char *otpfile = NULL;
	uint32_t value;
	uint8_t encrypt_type, hash_type, sign_type;
	uint16_t pub_key_len, decrypt_key_len;
	const uint8_t *pub_key_data, *decrypt_key;
	uint8_t *hash = NULL, boot_config = 0;
	int ret, hash_len;
	security_t boot2_security;
	uint16_t otp_udata_len;
	const uint8_t *otp_udata;

	/* Get boot2 encryption configuration */
	ret = get_boot2_encrypt_type(&encrypt_type);
	sboot_assert((ret == 0), OTP_CLEANUP_FUNC, ret, "");

	if (encrypt_type != NO_ENCRYPT) {
		boot_config |= OTP_ENCRYPTED_BOOT;
		if (tlv_store_get_ref(config_store, KEY_BOOT2_DECRYPT_KEY,
					&decrypt_key_len, &decrypt_key) <= 0)
			/* If no separate decrypt key is found, assume it
			 * be same as encrypt key */
			ret = tlv_store_get_ref(config_store,
					KEY_BOOT2_ENCRYPT_KEY,
					&decrypt_key_len, &decrypt_key);
			sboot_assert((ret >= 0), OTP_CLEANUP_FUNC,
			ERR_NO_ENCRYPT_KEY,
			"Boot2 encryption key is not specified");
	} else {
		decrypt_key = NULL;
		decrypt_key_len = 0;
	}

	/* Get boot2 signing configuration */
	ret = get_boot2_sign_type(&sign_type);
	sboot_assert((ret == 0), OTP_CLEANUP_FUNC, ret, "");

	if (sign_type != NO_SIGN) {
		boot_config |= OTP_SIGNED_BOOT;
		ret = tlv_store_get_ref(config_store, KEY_BOOT2_PUBLIC_KEY,
					&pub_key_len, &pub_key_data);
		sboot_assert((ret >= 0), OTP_CLEANUP_FUNC, ERR_NO_PUBLIC_KEY,
			"Boot2 public key is not specified");

		/* Get hash configuration */
		ret = get_boot2_hash_type(&hash_type);
		sboot_assert((ret == 0), OTP_CLEANUP_FUNC, ret, "");

		initialize_hash_context(&boot2_security, hash_type);

		/* Calculate the hash of signing public key */
		ret = boot2_security.hash_init(&boot2_security);
		sboot_assert((ret >= 0), OTP_CLEANUP_FUNC, ERR_HASH_INIT,
			 "Failed to initialize hash algorithm: %d", ret);

		ret = boot2_security.hash_update(&boot2_security,
				pub_key_data, pub_key_len);
		sboot_assert((ret >= 0), OTP_CLEANUP_FUNC, ERR_HASH_UPDATE,
			"Failed to update hash value of boot2 public key: %d",
			ret);

		ret = boot2_security.hash_finish(&boot2_security,
				(void **)&hash);
		sboot_assert((ret >= 0), OTP_CLEANUP_FUNC, ERR_HASH_FINISH,
			"Failed to finalize hash value of boot2 public key: %d",
			ret);

		hash_len = ret;
	} else {
		hash = NULL;
		hash_len = 0;
	}

	/* Get boot config flags */
	ret = tlv_store_get(config_store, KEY_OTP_JTAG_ENABLE,
				sizeof(value), (uint8_t *)&value);
	if (ret < 0)
		value = true;
	boot_config |= (value ? OTP_JTAG_ENABLE : OTP_JTAG_DISABLE);

	ret = tlv_store_get(config_store, KEY_OTP_UART_USB_BOOT_ENABLE,
				sizeof(value), (uint8_t *)&value);
	if (ret < 0)
		value = true;
	boot_config |= (value ? OTP_USB_UART_BOOT_ENABLE
			: OTP_USB_UART_BOOT_DISABLE);

	/* Get OTP user data */
	ret = tlv_store_get_ref(config_store, KEY_OTP_USER_DATA,
				&otp_udata_len, &otp_udata);

	/* OTP configuration file is created in all cases */
	otpfile = get_outdir_filename(infile, NULL);
	sboot_assert(otpfile, OTP_CLEANUP_FUNC, ERR_BAD_OUTFILE_NAME, "");

	otp = fopen(otpfile, "w");
	sboot_assert(otp, OTP_CLEANUP_FUNC, ERR_FILE_OPEN,
			"Could not open otp configuration file %s for write:"
			" %s", otpfile, strerror(errno));

	ret = otp_write_header(otp);
	sboot_assert((ret >= 0), OTP_CLEANUP_FUNC, ERR_FILE_WRITE,
		"Could not write header to otp configuration file %s: %s",
		otpfile, strerror(errno));

	ret = otp_write_boot_config(otp, boot_config);
	sboot_assert((ret >= 0), OTP_CLEANUP_FUNC, ERR_FILE_WRITE,
		"Could not write boot config to otp configuration file %s: %s",
		otpfile, strerror(errno));

	ret = otp_write_decrypt_key(otp, decrypt_key, decrypt_key_len);
	sboot_assert((ret >= 0), OTP_CLEANUP_FUNC, ERR_FILE_WRITE,
		"Could not write decryption key to otp configuration file"
		" %s: %s", otpfile, strerror(errno));

	ret = otp_write_public_key_hash(otp, hash, hash_len);
	sboot_assert((ret >= 0), OTP_CLEANUP_FUNC, ERR_FILE_WRITE,
		"Could not write public key hash to otp configuration file"
		" %s: %s", otpfile, strerror(errno));

	ret = otp_write_user_data(otp, otp_udata, otp_udata_len);
	sboot_assert((ret >= 0), OTP_CLEANUP_FUNC, ERR_FILE_WRITE,
		"Could not write user data to otp configuration file"
		" %s: %s", otpfile, strerror(errno));

	otp_close_file(otp);
	otp  = NULL;

	sboot_v("Created the otp configuration file %s", otpfile);

	OTP_CLEANUP_FUNC;

	return 0;
}

#if 0
/* This code generates otp file in labtool format */

/* This is what kingletOtpTool does i.e. it changes the endianness of data
 * before calculating crc and endianness of crc itself.
 */
static uint32_t get_crc32(const uint8_t *data, uint32_t len)
{
	uint8_t *buf;
	uint32_t crc, result, i;

	buf = malloc(len);
	sboot_assert(buf, , 0, "");

	for (i = 0; i < len / 4; i++) {
		buf[i * 4] = data[i * 4 + 3];
		buf[i * 4 + 1] = data[i * 4 + 2];
		buf[i * 4 + 2] = data[i * 4 + 1];
		buf[i * 4 + 3] = data[i * 4];
	}

	crc = soft_crc32(buf, len, 0);

	*((uint8_t *)&result) = *((uint8_t *)&crc + 3);
	*((uint8_t *)&result + 1) = *((uint8_t *)&crc + 2);
	*((uint8_t *)&result + 2) = *((uint8_t *)&crc + 1);
	*((uint8_t *)&result + 3) = *((uint8_t *)&crc);

	free(buf);
	return result;
}

int otp_write_decrypt_key(FILE *otp, const uint8_t *key, uint32_t len)
{
	uint8_t decrypt_key[40];
	uint32_t crc, i;
	char hex[128];
	int ret;

	if (len == 0 || key == NULL)
		return 0;

	memset(decrypt_key, 0, sizeof(decrypt_key));
	/* Byte 0 is signature of OTP line, Byte 1 is length of OTP data
	 * in 32-byte lines. Refer to MW30X Bootrom Functional Spec 0.8 Section
	 * 5 (OTP data format) for details.
	 */
	decrypt_key[0] = 0x0A;
	decrypt_key[1] = 0x05;
	memcpy(&decrypt_key[2], key, len);

	crc = get_crc32(key, len);

	memcpy(&decrypt_key[len + 2], &crc, sizeof(crc));
	bin2hex(decrypt_key, hex, sizeof(decrypt_key), sizeof(hex));

	for (i = 0; i < 80; i += 16) {
		ret = fwrite(hex + i, 1, 16, otp);
		sboot_assert((ret == 16), , -errno, "");
		fwrite("\n", 1, 1, otp);
	}

	return 0;
}

int otp_write_public_key_hash(FILE *otp, const uint8_t *hash, uint32_t len)
{
	uint8_t key_hash[40];
	uint32_t crc, i;
	char hex[81];
	int ret;

	if (len == 0 || hash == NULL)
		return 0;

	memset(key_hash, 0, sizeof(key_hash));
	/* Byte 0 is signature of OTP line, Byte 1 is length of OTP data
	 * in 32-byte lines. Refer to MW30X Bootrom Functional Spec 0.8 Section
	 * 5 (OTP data format) for details.
	 */
	key_hash[0] = 0x0E;
	key_hash[1] = 0x05;
	memcpy(&key_hash[2], hash, len);

	crc = get_crc32(hash, len);

	memcpy(&key_hash[len + 2], &crc, sizeof(crc));
	bin2hex(key_hash, hex, sizeof(key_hash), sizeof(hex));

	for (i = 0; i < 80; i += 16) {
		ret = fwrite(hex + i, 1, 16, otp);
		sboot_assert((ret == 16), , -errno, "");
		fwrite("\n", 1, 1, otp);
	}

	return 0;
}

int otp_write_boot_config(FILE *otp, uint8_t boot_config)
{
	uint8_t boot_line[8];
	char hex[17];
	int ret;

	memset(boot_line, 0, sizeof(boot_line));
	/* Byte 0 is signature of OTP line, Byte 1 is length of OTP data
	 * in 32-byte lines. Refer to MW30X Bootrom Functional Spec 0.8 Section
	 * 5 (OTP data format) for details.
	 */
	boot_line[0] = 0xC;
	boot_line[1] = 1;
	boot_line[2] = boot_config;

	bin2hex(boot_line, hex, sizeof(boot_line), sizeof(hex));

	ret = fwrite(hex, 1, 16, otp);
	sboot_assert((ret == 16), , -errno, "");
	fwrite("\n", 1, 1, otp);

	return 0;
}

int otp_write_user_data(FILE *otp, const uint8_t *udata, uint32_t len)
{
	/* TODO: Add support. At the moment don't know the 0th byte
	 * (signature) for OTP user data
	 */
	return -1;
}

int otp_write_header(FILE *otp)
{
	return 0;
}

int otp_close_file(FILE *otp)
{
	fclose(otp);
	return 0;
}
#endif
