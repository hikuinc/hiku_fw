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
#include <firmware_structure.h>
#include <secure_boot.h>
#include <soft_crc.h>

#include "utils.h"
#include "security.h"

#define FW_CLEANUP_FUNC fw_cleanup(buf, outbuf, hash, sign, signout, \
					mic, sign_mic, outfile, fp2)

static void fw_cleanup(void *buf, void *outbuf, void *hash, void *sign,
		void *signout, void *mic, void *sign_mic, void *outfile,
		FILE *of)
{
	if (buf)
		free(buf);
	if (outbuf)
		free(outbuf);
	if (hash)
		free(hash);
	if (sign)
		free(sign);
	if (signout)
		free(signout);
	if (mic)
		free(mic);
	if (sign_mic)
		free(sign_mic);
	if (outfile)
		free(outfile);
	if (of)
		fclose(of);
}

static uint8_t check_for_xip(uint8_t *buf, char *infile)
{
	struct img_hdr *ih = (struct img_hdr *)buf;
	uint8_t valid_fw = false;

	valid_fw = (ih->magic_str == FW_MAGIC_STR &&
			ih->magic_sig == FW_MAGIC_SIG);
	if (!valid_fw)
		return false;

	/* Is XIP? */
	if ((ih->entry  & 0x1f000000) == 0x1f000000)
		return true;

	return false;
}

/* Get encryption configuration */
int get_fw_encrypt_type(uint8_t *pencrypt_type)
{
	int ret = 0;

	ret = tlv_store_get(config_store, KEY_FW_ENCRYPT_ALGO, 1,
				pencrypt_type);
	if (ret < 0)
		*pencrypt_type = NO_ENCRYPT;

	ret = FIRMWARE_SUPPORTS_ENCRYPT_ALGO(*pencrypt_type);
	sboot_assert(ret, , ERR_UNSUPPORTED_ENCRYPT_ALGO,
		"Unsupported encryption algorithm specified for firmware");

	return 0;
}

/* Get hash configuration */
int get_fw_hash_type(uint8_t *phash_type)
{
	int ret = 0;

	ret = tlv_store_get(config_store, KEY_FW_HASH_ALGO, 1,
			phash_type);
	sboot_assert((ret > 0), , ERR_NO_HASH,
		"Firmware hash algorithm for signing is not specified");

	ret = FIRMWARE_SUPPORTS_HASH_ALGO(*phash_type);
	sboot_assert(ret, , ERR_NO_HASH,
		"Unsupported hash algorithm specified for firmware"
		" signing");

	return 0;
}

/* Get signing configuration */
int get_fw_sign_type(uint8_t *psign_type)
{
	int ret = 0;

	ret = tlv_store_get(config_store, KEY_FW_SIGNING_ALGO, 1, psign_type);
	if (ret < 0)
		*psign_type = NO_SIGN;

	ret = FIRMWARE_SUPPORTS_SIGNING_ALGO(*psign_type);
	sboot_assert(ret, , ERR_UNSUPPORTED_SIGN_ALGO,
		"Unsupported signature algorithm specified for firmware");

	return 0;
}

/* Generate secure mcu firmware */
int generate_fw(char *infile)
{
	FILE *fp2;
	int    ret, len, mic_len;
	uint8_t sec_hdr[SB_SEC_IMG_HDR_SIZE], value, xip;
	uint32_t out_len, hash_len;
	uint32_t sign_len, signout_len;
	uint16_t sign_key_len, encrypt_key_len, iv_len = 0;
	security_t fw_security;
	uint8_t encrypt_type, hash_type, sign_type;
	uint8_t *buf, *outbuf, iv[16];
	const uint8_t *sign_key_data, *encrypt_key;
	uint8_t *hash, *sign, *signout, *mic, *sign_mic;
	char *outfile, hex[513];

	buf = outbuf = hash = sign = signout = mic = sign_mic = NULL;
	fp2 = NULL;
	outfile = NULL;
	signout_len = 0;

	/* Get encryption configuration */
	ret = get_fw_encrypt_type(&encrypt_type);
	sboot_assert((ret == 0), FW_CLEANUP_FUNC, ret, "");

	initialize_encryption_context(&fw_security, encrypt_type);

	/* Get signing configuration */
	ret = get_fw_sign_type(&sign_type);
	sboot_assert((ret == 0), FW_CLEANUP_FUNC, ret, "");

	initialize_signing_context(&fw_security, sign_type);

	/* return if nothing to do */
	if ((encrypt_type == NO_ENCRYPT) && (sign_type == NO_SIGN)
			&& (outdir == NULL)) {
		sboot_v("No encryption or signing was asked for firmware");
		sboot_v("Doing nothing for firmware image");
		return 0;
	}

	/* Get encryption parameters */
	if (encrypt_type != NO_ENCRYPT) {
		ret = tlv_store_get_ref(config_store, KEY_FW_ENCRYPT_KEY,
				&encrypt_key_len, &encrypt_key);
		sboot_assert((ret >= 0), FW_CLEANUP_FUNC, ERR_NO_ENCRYPT_KEY,
			"Firmware encryption key is not specified");

		ret = tlv_store_get(config_store, KEY_FW_NONCE,
				sizeof(iv), iv);
		sboot_assert((ret != ERR_TLV_BAD_BUF_LEN), FW_CLEANUP_FUNC,
			ERR_NO_NONCE, "Firmware iv/nonce is too long");

		if (ret == ERR_TLV_NOT_FOUND)
			/* Generate random nonce later*/
			iv_len = 0;
		else
			iv_len = ret;
	}

	/* Get signing parameters */
	if (sign_type != NO_SIGN) {
		/* Get hash configuration */
		ret = get_fw_hash_type(&hash_type);
		sboot_assert((ret == 0), FW_CLEANUP_FUNC, ret, "");

		initialize_hash_context(&fw_security, hash_type);

		ret = tlv_store_get_ref(config_store, KEY_FW_PUBLIC_KEY,
				&sign_key_len, &sign_key_data);
		sboot_assert((ret >= 0), FW_CLEANUP_FUNC, ERR_NO_PUBLIC_KEY,
			"Firmware public key is not specified");

		ret = tlv_store_get_ref(config_store, KEY_FW_PRIVATE_KEY,
				&sign_key_len, &sign_key_data);
		sboot_assert((ret >= 0), FW_CLEANUP_FUNC, ERR_NO_PRIVATE_KEY,
			"Firmware private key is not specified");
	}

	/* Verify if a .bin file and get output file name */
	outfile = get_outfile_name(infile, 0, 0, encrypt_type, sign_type);
	sboot_assert(outfile, FW_CLEANUP_FUNC, ERR_BAD_OUTFILE_NAME, "");

	/* Get firmware file data */
	len = file_read(infile, &buf);
	sboot_assert((len >= 0), FW_CLEANUP_FUNC, ERR_FILE_READ,
			"Could not read firmware file %s: %s",
			infile, strerror(errno));

	sboot_v("Read the firmware image %s of size %d bytes", infile, len);

	/* Check if firmware is XIP and encryption is requested */
	xip = check_for_xip(buf, infile);

	/* encryption part */
	if (encrypt_type != NO_ENCRYPT) {
		sboot_assert((xip == false), FW_CLEANUP_FUNC,
			ERR_INVALID_FW_FILE,
			"Encryption of XIP firmware is not allowed");

		/* Encrypt the firmware image data */
		 if (iv_len == 0) {
			/* Generate random nonce if not specified */
			iv_len = fw_security.encrypt_query(ENC_IV_LEN);
			ret = PlatformRandomBytes(iv, iv_len);
			sboot_assert((ret == 0),
					FW_CLEANUP_FUNC, ERR_NO_NONCE,
					"Failed to generate random iv/nonce");
			sboot_v("Firmware iv/nonce len: %d", iv_len);
			sboot_v("Firmware iv/nonce: %s",
				bin2hex(iv, hex, iv_len, sizeof(hex)));
		}

		ret = fw_security.encrypt_init(&fw_security, encrypt_key,
				encrypt_key_len, iv, iv_len);
		sboot_assert((ret >= 0), FW_CLEANUP_FUNC, ERR_ENCRYPT_INIT,
			"Failed to initialize encryption algorithm: %d", ret);

		mic = NULL, mic_len = 0;
		ret = fw_security.encrypt(&fw_security, buf, len,
				(void **)&outbuf, (void **)&mic, &mic_len);
		sboot_assert((ret >= 0), FW_CLEANUP_FUNC, ERR_ENCRYPT_DATA,
			"Failed to encrypt firmware: %d", ret);

		if (mic_len) {
			sboot_v("Firmware mic size: %d", mic_len);
			sboot_v("Firmware mic: %s",
				bin2hex(mic, hex, mic_len, sizeof(hex)));
		}

		if (outbuf != buf)
			free(buf);
		out_len = ret;
	} else {
		outbuf = buf;
		out_len = len;
	}

	/* either buf is freed or assigned to outbuf */
	buf = NULL;

	/* Signing part */
	if (sign_type != NO_SIGN) {
		/* Calculate the hash of firmware */
		ret = fw_security.hash_init(&fw_security);
		sboot_assert((ret >= 0), FW_CLEANUP_FUNC, ERR_HASH_INIT,
			"Failed to initialize hash algorithm: %d", ret);

		ret = fw_security.hash_update(&fw_security, outbuf, out_len);
		sboot_assert((ret >= 0), FW_CLEANUP_FUNC, ERR_HASH_UPDATE,
			"Failed to update hash value of firmware: %d", ret);

		ret = fw_security.hash_finish(&fw_security, (void **)&hash);
		sboot_assert((ret >= 0), FW_CLEANUP_FUNC, ERR_HASH_FINISH,
			"Failed to finalize hash value of firmware: %d", ret);

		hash_len = ret;
		sboot_v("Hash of firmware: %s", bin2hex(hash, hex,
				hash_len, sizeof(hex)));

		/* Get the signature of hash of firmware */
		ret = fw_security.sign_init(&fw_security, sign_key_data,
				sign_key_len);
		sboot_assert((ret >= 0), FW_CLEANUP_FUNC, ERR_SIGNATURE_INIT,
			"Failed to initialize signature algorithm: %d", ret);

		ret = fw_security.sign(&fw_security, hash, hash_len,
				(void **)&sign);
		sboot_assert((ret >= 0), FW_CLEANUP_FUNC,
			ERR_SIGNATURE_GENERATION,
			"Failed to sign the firmware: %d", ret);

		sign_len = ret;

		free(hash);
		hash = NULL;

		sboot_v("Firmware signature size: %d", ret);
		sboot_v("Firmware signature: %s",
			bin2hex(sign, hex, sign_len, sizeof(hex)));

		fw_security.sign_deinit(&fw_security);

		signout = sign;
		signout_len = sign_len;

		/* sign is either freed or assigned to signout */
		sign = NULL;
	}

	tlv_store_init(sec_hdr, sizeof(sec_hdr), IMAGE_SECURE_HEADER);

#ifdef TEST_BUILD
	corrupt_sec_hdr_keys(sec_hdr);
#endif
	if (encrypt_type != NO_ENCRYPT || sign_type != NO_SIGN) {
		/* Write image length */
		tlv_store_add(sec_hdr, KEY_FW_IMAGE_LEN, sizeof(out_len),
				(uint8_t *)&out_len);
	}

	if (encrypt_type != NO_ENCRYPT) {
		/* Write Nonce to header */
		tlv_store_add(sec_hdr, KEY_FW_NONCE, iv_len, iv);

		/* Write mic if generated */
		if (mic != NULL && mic_len != 0)
			tlv_store_add(sec_hdr, KEY_FW_MIC, mic_len, mic);
	}

	if (sign_type != NO_SIGN) {
		/* Write the Signature to the header */
		tlv_store_add(sec_hdr, KEY_FW_SIGNATURE, signout_len, signout);

		/* For XIP execution, firmware image (after secure header)
		 * must start on 4-byte alignment. This is the requirement
		 * of flash controller. To achieve this, we pad secure header
		 * till its size is a multiple of 4-bytes.
		 */
		if (xip == true) {
			value = 0xFF;
			tlv_store_xip_padding(sec_hdr, &value);
		}
	}

	tlv_store_close(sec_hdr, soft_crc32);
#ifdef TEST_BUILD
	corrupt_sec_hdr(sec_hdr);
#endif

	/* Write header and firmware data to output file */
	fp2 = fopen(outfile, "wb");
	sboot_assert(fp2, FW_CLEANUP_FUNC, ERR_FILE_OPEN,
		"Could not open secure firmware file %s for write: %s",
		outfile, strerror(errno));

	/* Write secure image header to the file */
	len = tlv_store_length(sec_hdr);
	if (len > sizeof(img_sec_hdr)) {
		ret = fwrite(sec_hdr, 1, len, fp2);
		sboot_assert((ret == len), FW_CLEANUP_FUNC, ERR_FILE_WRITE,
			"Could not write header to secure firmware file %s: %s",
			outfile, strerror(errno));
	}

	/* Write the firmware data to the output file */
	ret = fwrite(outbuf, 1, out_len, fp2);
	sboot_assert((ret == out_len), FW_CLEANUP_FUNC, ERR_FILE_WRITE,
		"Could not write image data to secure firmware file %s: %s",
		outfile, strerror(errno));

	sboot_v("Created the secure firmware image %s", outfile);

	FW_CLEANUP_FUNC;

	return 0;
}
