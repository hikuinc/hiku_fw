/*
 * Copyright (C) 2015, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <secure_boot.h>
#include <soft_crc.h>

#include "utils.h"

uint8_t config_store[MAX_CONFIG_STORE_SIZE];

#define FIELD_SEPARATORS ";,\t"

typedef struct {
	char *keyword;
	uint8_t id;
} keymap;

/* Add new TLV type keywords here */
static keymap tlv_type_keymap[] = {
	{"KEY_OTP_JTAG_ENABLE", KEY_OTP_JTAG_ENABLE},
	{"KEY_OTP_UART_USB_BOOT_ENABLE", KEY_OTP_UART_USB_BOOT_ENABLE},
	{"KEY_OTP_USER_DATA", KEY_OTP_USER_DATA},

	{"KEY_BOOT2_LOAD_ADDR", KEY_BOOT2_LOAD_ADDR},
	{"KEY_BOOT2_ENTRY_ADDR", KEY_BOOT2_ENTRY_ADDR},

	{"KEY_BOOT2_SIGNING_ALGO", KEY_BOOT2_SIGNING_ALGO},
	{"KEY_BOOT2_HASH_ALGO", KEY_BOOT2_HASH_ALGO},
	{"KEY_BOOT2_ENCRYPT_ALGO", KEY_BOOT2_ENCRYPT_ALGO},
	{"KEY_BOOT2_PRIVATE_KEY", KEY_BOOT2_PRIVATE_KEY},
	{"KEY_BOOT2_PUBLIC_KEY", KEY_BOOT2_PUBLIC_KEY},
	{"KEY_BOOT2_ENCRYPT_KEY", KEY_BOOT2_ENCRYPT_KEY},
	{"KEY_BOOT2_DECRYPT_KEY", KEY_BOOT2_DECRYPT_KEY},
	{"KEY_BOOT2_NONCE", KEY_BOOT2_NONCE},

	{"KEY_MCUFW_SIGNING_ALGO", KEY_FW_SIGNING_ALGO},
	{"KEY_MCUFW_HASH_ALGO", KEY_FW_HASH_ALGO},
	{"KEY_MCUFW_ENCRYPT_ALGO", KEY_FW_ENCRYPT_ALGO},
	{"KEY_MCUFW_PRIVATE_KEY", KEY_FW_PRIVATE_KEY},
	{"KEY_MCUFW_PUBLIC_KEY", KEY_FW_PUBLIC_KEY},
	{"KEY_MCUFW_ENCRYPT_KEY", KEY_FW_ENCRYPT_KEY},
	{"KEY_MCUFW_DECRYPT_KEY", KEY_FW_DECRYPT_KEY},
	{"KEY_MCUFW_NONCE", KEY_FW_NONCE},
};

static int tlv_type_keymap_count = (sizeof(tlv_type_keymap) / sizeof(keymap));

/* Add new TLV value format here */
enum {
	KEY_FORMAT_HEX = 0,
	KEY_FORMAT_DEC,
	KEY_FORMAT_BIN,
	KEY_FORMAT_STR,
	KEY_FORMAT_FILE
};

static keymap tlv_format_keymap[] = {
	{"HEX", KEY_FORMAT_HEX},
	{"DEC", KEY_FORMAT_DEC},
	{"STR", KEY_FORMAT_STR},
	{"FILE", KEY_FORMAT_FILE}
};

static int tlv_format_keymap_count =
			(sizeof(tlv_format_keymap) / sizeof(keymap));

/* Add new TLV value keywords here */
static keymap tlv_value_keymap[] = {
	{"NONE", NO_ENCRYPT},
	{"0", NO_ENCRYPT},
	{"NO_ENCRYPT", NO_ENCRYPT},
	{"AES_CCM_256_ENCRYPT", AES_CCM_256_ENCRYPT},
	{"AES_CTR_128_ENCRYPT", AES_CTR_128_ENCRYPT},

	{"NO_HASH", NO_HASH},
	{"SHA256_HASH", SHA256_HASH},

	{"NO_SIGN", NO_SIGN},
	{"RSA_SIGN", RSA_SIGN},
};

static int tlv_value_keymap_count = (sizeof(tlv_value_keymap) / sizeof(keymap));

/* Find the keyword id corresponding to string */
static int parse_token(char *tok, const keymap *map, int map_size)
{
	int i;

	for (i = 0; i < map_size; i++)
		if (!strncasecmp(tok, map[i].keyword, strlen(map[i].keyword)))
			return map[i].id;

	return ERR_TLV_INVALID_TYPE;
}

static char *ignore_white_spaces(char *tok)
{
	while (tok && *tok != '\0') {
		if (!isspace((int) *tok))
			return tok;
		tok++;
	}

	return NULL;
}

/* parse each single line of config file */
static int parse_line(char *line)
{
	char *tok;
	uint8_t *buf;
	int32_t tlv_type, len, format;

	/* ignore comments */
	if (line[0] == '#')
		return 0;

	/* Parse TLV type field */
	tok = strtok(line, FIELD_SEPARATORS);
	if (!tok || !*tok) {
		sboot_w("Invalid config file format: %s", line);
		return ERR_INVALID_CONFIG_FILE;
	}

	tok = ignore_white_spaces(tok);
	if (!tok || !*tok)
		/* empty line allowed */
		return 0;

	tlv_type = parse_token(tok, tlv_type_keymap, tlv_type_keymap_count);
	if (tlv_type < 0 || !IS_VALID_TLV_TYPE(tlv_type)) {
		/* Try to see if tlv type is mentioned as decimal */
		len = dec2bin(tok, &tlv_type, sizeof(tlv_type));
		if (len <= 0 || tlv_type < KEY_APP_DATA_START ||
				tlv_type > KEY_APP_DATA_END) {
			sboot_w("Invalid field: %s", line);
			return ERR_TLV_INVALID_TYPE;
		}
	}

	/* Parse TLV value format field */
	tok = strtok(NULL, FIELD_SEPARATORS "\n");
	tok = ignore_white_spaces(tok);
	if (!tok || !*tok) {
		sboot_w("No format for the field: %s", line);
		return ERR_INVALID_CONFIG_FILE;
	}

	format = parse_token(tok, tlv_format_keymap, tlv_format_keymap_count);

	if (format < 0) {
		sboot_w("Invalid format for the field: %s", line);
		return ERR_INVALID_CONFIG_FILE;
	}

	/* Parse TLV value field as per value format */
	tok = strtok(NULL, FIELD_SEPARATORS "\n");
	tok = ignore_white_spaces(tok);
	if (!tok || !*tok) {
		sboot_w("No value for the field: %s", line);
		return ERR_INVALID_CONFIG_FILE;
	}

	/* Value is a file */
	if (format == KEY_FORMAT_FILE) {
		len = file_read(tok, &buf);
		if (len < 0) {
			sboot_w("Could not open field file %s: %s",
				tok, strerror(errno));
			return ERR_INVALID_CONFIG_FILE;
		}
		tlv_store_add(config_store, (uint8_t)tlv_type, len, buf);
		free(buf);
		return 0;
	}

	/* Value is a hex string */
	if (format == KEY_FORMAT_HEX) {
		len = strlen(tok);

		buf = malloc((len + 1) / 2);
		sboot_assert(buf, , ERR_INSUFFICIENT_MEMORY,
			"Could not allocate memory: %s", line);

		len = hex2bin(tok, buf, len);
		if (len <= 0) {
			free(buf);
			sboot_w("Invalid value for the field: %s", line);
			return ERR_INVALID_CONFIG_FILE;
		}
		tlv_store_add(config_store, (uint8_t)tlv_type, len, buf);

		free(buf);
	}

	/* Value is an ascii string */
	if (format == KEY_FORMAT_STR) {
		int value;
		value = parse_token(tok, tlv_value_keymap,
					tlv_value_keymap_count);

		if (format >= 0) {
			/* It's a keyword */
			tlv_store_add(config_store, (uint8_t)tlv_type, 1,
					(uint8_t *) &value);
		} else
			tlv_store_add(config_store, (uint8_t)tlv_type,
					strlen(tok), (uint8_t *)tok);
	}

	/* Value is a binary string */
	if (format == KEY_FORMAT_BIN) {
		len = strlen(tok);

		buf = malloc((len + 7) / 8);
		sboot_assert(buf, , ERR_INSUFFICIENT_MEMORY,
			"Could not allocate memory: %s", line);

		len = binstr2bin(tok, buf, len);
		if (len <= 0) {
			free(buf);
			sboot_w("Invalid value for the field: %s", line);
			return ERR_INVALID_CONFIG_FILE;
		}
		tlv_store_add(config_store, (uint8_t)tlv_type, len, buf);

		free(buf);
	}

	/* Value is a decimal integer */
	if (format == KEY_FORMAT_DEC) {
		int value;

		len = dec2bin(tok, &value, sizeof(value));
		if (len <= 0) {
			sboot_w("Invalid value for the field: %s", line);
			return ERR_INVALID_CONFIG_FILE;
		}
		tlv_store_add(config_store, (uint8_t)tlv_type, len,
				(uint8_t *) &value);
	}

	return 0;
}

/* Parse the config file */
int parse_config_file(char *config_file)
{
	char line[MAX_CONFIG_LINE_SIZE];

	FILE *cfp = fopen(config_file, "r");
	sboot_assert(cfp, , ERR_FILE_OPEN, "Could not open config file %s: %s",
			config_file, strerror(errno));

	tlv_store_init(config_store, sizeof(config_store), CONFIG_FILE_STORE);

	while (fgets(line, sizeof(line), cfp))
		parse_line(line);

	tlv_store_close(config_store, soft_crc32);
	fclose(cfp);

#ifdef DEBUG_CONFIG_FILE
	validate_store(config_store);
	sboot_v("Total config store size: %d", tlv_store_length(config_store));
#endif

	return 0;
}

#ifdef DEBUG_CONFIG_FILE
int validate_store(uint8_t *tlv_buf)
{
	uint8_t test[2048];
	uint8_t buf[2048];
	uint32_t i, len;

	tlv_store_init(test, sizeof(config_store), CONFIG_FILE_STORE);

	len = tlv_store_get(tlv_buf, 0, sizeof(buf), buf);
	tlv_store_add(test, 0, len, buf);
	len = tlv_store_get(tlv_buf, 1, sizeof(buf), buf);
	tlv_store_add(test, 1, len, buf);
	len = tlv_store_get(tlv_buf, 2, sizeof(buf), buf);
	tlv_store_add(test, 2, len, buf);
	len = tlv_store_get(tlv_buf, 3, sizeof(buf), buf);
	tlv_store_add(test, 3, len, buf);
	len = tlv_store_get(tlv_buf, 4, sizeof(buf), buf);
	tlv_store_add(test, 4, len, buf);

	tlv_store_close(test);

	sboot_v("Test len = %d", tlv_store_length(test));

	for (i = 0; i < tlv_store_length(config_store); i++)
		if (test[i] != config_store[i])
			sboot_e("TLV Reads failed %d", i);

	return 0;
}
#endif
