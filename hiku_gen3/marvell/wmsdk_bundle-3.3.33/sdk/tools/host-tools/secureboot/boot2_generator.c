/*
 * Copyright (C) 2015, Marvell International Ltd.
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
#include <errno.h>
#include <bootrom_header.h>
#include <secure_boot.h>
#include <soft_crc.h>

#include "security.h"
#include "utils.h"

#define ALIGN_SIZE 16

uint8_t keystore[SB_KEYSTORE_SIZE];

#define BOOT2_CLEANUP_FUNC boot2_cleanup(buf, in_buf, (outbuf == (void *)hdr ? \
			NULL : outbuf), hash, sign, mic, bootfile, fp2)

static void boot2_cleanup(void *buf, void *in_buf, void *outbuf, void *hash,
		void *sign, void *mic, void *bootfile, FILE *of)
{
	if (buf)
		free(buf);
	if (in_buf)
		free(in_buf);
	if (outbuf)
		free(outbuf);
	if (hash)
		free(hash);
	if (sign)
		free(sign);
	if (mic)
		free(mic);
	if (bootfile)
		free(bootfile);
	if (of)
		fclose(of);
}

int generate_keystore()
{
	uint8_t value, *tlv_store_ptr;
	uint16_t len;
	const uint8_t *buf;
	int ret;

	tlv_store_init(keystore, sizeof(keystore), BOOT2_KEYSTORE);

	/* Get the signing parameters for firmware */
	if ((tlv_store_get(config_store, KEY_FW_SIGNING_ALGO, 1, &value) > 0)
			&& (value != NO_SIGN)) {
		tlv_store_add(keystore, KEY_FW_SIGNING_ALGO, 1, &value);

		if ((tlv_store_get(config_store, KEY_FW_HASH_ALGO, 1,
					&value) > 0) && (value != NO_HASH))
			tlv_store_add(keystore, KEY_FW_HASH_ALGO, 1, &value);
		else {
			sboot_e("Firmware Hash algorithm is not specified");
			return ERR_KEYSTORE_GEN;
		}

		if (tlv_store_get_ref(config_store, KEY_FW_PUBLIC_KEY, &len,
					&buf) > 0)
			tlv_store_add(keystore, KEY_FW_PUBLIC_KEY, len, buf);
		else {
			sboot_e("Firmware Public key is not specified");
			return ERR_KEYSTORE_GEN;
		}
	}

	/* Get the encryption parameters for firmware */
	if ((tlv_store_get(config_store, KEY_FW_ENCRYPT_ALGO, 1, &value) > 0)
			&& (value != NO_ENCRYPT)) {
		tlv_store_add(keystore, KEY_FW_ENCRYPT_ALGO, 1, &value);

		if (tlv_store_get_ref(config_store, KEY_FW_DECRYPT_KEY, &len,
					&buf) > 0)
			tlv_store_add(keystore, KEY_FW_DECRYPT_KEY, len, buf);
		else {
			/* If separate decrypt key is not found, assume it to
			 * be same as encrypt key */
			if (tlv_store_get_ref(config_store, KEY_FW_ENCRYPT_KEY,
					&len, &buf) > 0)
				tlv_store_add(keystore, KEY_FW_DECRYPT_KEY,
						len, buf);
			else {
				sboot_e("Firmware Decryption key is not"
					" specified");
				return ERR_KEYSTORE_GEN;
			}
		}
	}

	/* Get PSM and application specific parameters to be stored */
	tlv_store_ptr = config_store;
	while (tlv_store_iterate_ref(tlv_store_ptr, &value,
			&len, &buf) >= 0) {
		tlv_store_ptr = NULL;

		if (value < KEY_APP_DATA_START || value > KEY_APP_DATA_END)
			continue;

		ret = tlv_store_add(keystore, value, len, buf);
		sboot_assert((ret == TLV_SUCCESS), , ERR_KEYSTORE_GEN,
				"Keystore size is greater than %u bytes",
				SB_KEYSTORE_SIZE);
	}

	tlv_store_close(keystore, soft_crc32);

#ifdef TEST_BUILD
	corrupt_keystore(keystore);
#endif
	return tlv_store_length(keystore);
}

#define KS_CLEANUP_FUNC ks_cleanup(keystore_file, fp)

static void ks_cleanup(void *keystore_file, FILE *fp)
{
	if (keystore_file)
		free(keystore_file);
	if (fp)
		fclose(fp);
}

int write_keystore_to_file(char *infile)
{
	FILE *fp = NULL;
	int ret, ks_len;
	char *keystore_file = NULL;

	/* Generate keystore only if not already created */
	if (tlv_store_validate(keystore, BOOT2_KEYSTORE, soft_crc32)
			!= TLV_SUCCESS)
		generate_keystore();

	ks_len = tlv_store_length(keystore);
	if (ks_len <= sizeof(boot2_ks_hdr)) {
		sboot_v("No keystore is required");
		sboot_v("Not creating any keystore file");
		return 0;
	}

	keystore_file = get_outdir_filename(infile, NULL);
	sboot_assert(keystore_file, , ERR_BAD_OUTFILE_NAME, "");

	fp = fopen(keystore_file, "w");
	sboot_assert(fp, KS_CLEANUP_FUNC, ERR_FILE_OPEN,
		"Could not open keystore file %s for write: %s",
		keystore_file, strerror(errno));

	ret = fwrite(keystore, 1, ks_len, fp);
	sboot_assert((ret == ks_len), KS_CLEANUP_FUNC, ERR_FILE_WRITE,
		"Could not write keystore file %s: %s",
		keystore_file, strerror(errno));

	sboot_v("Created the keystore file %s", keystore_file);

	KS_CLEANUP_FUNC;
	return 0;
}

int add_boot2_header(uint8_t **pbuf, int len)
{
	hdr_t h;
	uint8_t *buf = *pbuf;
	uint32_t codeSig;
	uint32_t es_len = sizeof(es_t);
	uint32_t hdr_len = sizeof(hdr_t);
	uint32_t mic_len = 16;

	/* Align length to 16 byte boundary for AES-CCM */
	uint32_t aligned_length = ((len + sizeof(codeSig) + ALIGN_SIZE - 1)
				& (~(ALIGN_SIZE - 1))) - sizeof(codeSig);

	uint32_t encrypt_len = es_len + hdr_len + aligned_length
				 + sizeof(codeSig) + mic_len;

	/* Reserved field in headers should be all 1's */
	memset(&h, 0xFF, sizeof(h));

	/* Start and entry addresses as per config file */
	if (tlv_store_get(config_store, KEY_BOOT2_LOAD_ADDR,
				sizeof(h.sh.destStartAddr),
				(uint8_t *)&h.sh.destStartAddr) < 0) {
		sboot_e("Boot2 load address is not specified");
		return ERR_NO_BOOT2_LOAD_ADDR;
	}
	if (tlv_store_get(config_store, KEY_BOOT2_ENTRY_ADDR,
				sizeof(h.bh.entryAddr),
				(uint8_t *)&h.bh.entryAddr) < 0) {
		sboot_e("Boot2 entry address is not specified");
		return ERR_NO_BOOT2_ENTRY_ADDR;
	}

	buf = realloc(*pbuf, encrypt_len);
	sboot_assert(buf, , ERR_INSUFFICIENT_MEMORY, "Failed to allocate"
				" memory for boot2 header");

	/* Return reallocated buffer address */
	*pbuf = buf;

	/* Enable DMA when loading code to SRAM */
	h.bh.commonCfg0.dmaEn = 0x1;
	/* Use RC32M for system clock */
	h.bh.commonCfg0.clockSel = 0x3;
	h.bh.commonCfg0.flashintfPrescalarOff = 0x1;
	h.bh.commonCfg0.flashintfPrescalar = 0b00100;
	/* Write magic code 'MRVL' */
	h.bh.magicCode = 0x4D52564C;
	/* Following are just default values, un-used ones */
	/* Refer to Bootrom Functional Specification 0.8 for details */
	h.bh.sfllCfg = 0xFE50787F;
	h.bh.flashcCfg0 = 0xFF053FF5;
	h.bh.flashcCfg1 = 0x07FFCC8C;
	h.bh.flashcCfg2 = 0x0;
	h.bh.flashcCfg3 = 0x0;
	/* Add CRC16 field */
	h.bh.CRCCheck = (uint16_t) soft_crc16(&h.bh,
				sizeof(h.bh) - sizeof(h.bh.CRCCheck), 0);

	/* Flash Section Header */
	/* Refer to Bootrom Functional Specification 0.8 for details */
	h.sh.codeLen = aligned_length;
	h.sh.bootCfg0.reg = 0xFFFFFFFF;
	h.sh.bootCfg1.reg = 0xFFFFFE3E;
	h.sh.CRCCheck =  (uint16_t) soft_crc16(&h.sh,
				sizeof(h.sh) - sizeof(h.sh.CRCCheck), 0);

	/* Move data, copy the headers, mic etc. Layout is :
	 * | es_t | hdr_t | boot2 code | alignmt padding | codeSig | mic | */
	memmove(buf + es_len + hdr_len, buf, len);
	memset(buf, 0x0, es_len);
	memcpy(buf + es_len, &h.bh, hdr_len);
	memset(buf + es_len + hdr_len + len, 0xff, aligned_length - len);

	codeSig = (uint16_t)soft_crc16(buf +  es_len + hdr_len, aligned_length, 0);
	memcpy(buf + es_len + hdr_len + aligned_length, &codeSig,
				sizeof(codeSig));
	memset(buf + encrypt_len - mic_len, 0x0, mic_len);

	return encrypt_len;
}

/* Get encryption configuration */
int get_boot2_encrypt_type(uint8_t *pencrypt_type)
{
	int ret = 0;

	ret = tlv_store_get(config_store, KEY_BOOT2_ENCRYPT_ALGO, 1,
				pencrypt_type);
	if (ret < 0)
		*pencrypt_type = NO_ENCRYPT;

	ret = BOOT2_SUPPORTS_ENCRYPT_ALGO(*pencrypt_type);
	if (ret == false) {
		sboot_w("Unsupported encryption algorithm specified for boot2");
		sboot_w("Defaulting to AES CCM-256 for boot2");
		*pencrypt_type = AES_CCM_256_ENCRYPT;
	}

	return 0;
}

/* Get hash configuration */
int get_boot2_hash_type(uint8_t *phash_type)
{
	int ret = 0;

	ret = tlv_store_get(config_store, KEY_BOOT2_HASH_ALGO, 1,
				phash_type);
	if (ret < 0)
		*phash_type = SHA256_HASH;

	ret = BOOT2_SUPPORTS_HASH_ALGO(*phash_type);
	if (ret == false) {
		sboot_w("Unsupported hash algorithm specified for"
			" boot2 signing");
		sboot_w("Defaulting to SHA256 hash for boot2");
		*phash_type = SHA256_HASH;
	}

	return 0;
}

/* Get signing configuration */
int get_boot2_sign_type(uint8_t *psign_type)
{
	int ret = 0;

	ret = tlv_store_get(config_store, KEY_BOOT2_SIGNING_ALGO, 1,
				psign_type);
	if (ret < 0)
		*psign_type = NO_SIGN;

	ret = BOOT2_SUPPORTS_SIGNING_ALGO(*psign_type);
	if (ret == false) {
		sboot_w("Unsupported signature algorithm specified for boot2");
		sboot_w("Defaulting to RSA signing for boot2");
		*psign_type = RSA_SIGN;
	}

	return 0;
}

int generate_boot2(char *infile, int add_header)
{
	int ret;
	FILE *fp2;
	char *bootfile, hex[513];
	uint32_t codeSig, boot2_len, aligned_length;
	uint8_t *buf, *in_buf;
	int ks_len, in_len, out_len, mic_len, hash_len, sign_len;
	uint16_t pub_key_len, sign_key_len, encrypt_key_len, iv_len = 0;
	uint8_t encrypt_type, hash_type, sign_type;
	uint8_t *boot2, *outbuf, iv[16];
	const uint8_t *pub_key_data, *sign_key_data, *encrypt_key;
	uint8_t *mic, *hash, *sign;
	security_t boot2_security;
	es_t *es = NULL;
	hdr_t *hdr = NULL;
	uint8_t aes_mic[16] = {0};

	buf = in_buf = outbuf = hash = sign = mic = NULL;
	bootfile = NULL;
	fp2 = NULL;

	/* Generate keystore */
	ks_len = generate_keystore();
	sboot_assert((ks_len > 0), BOOT2_CLEANUP_FUNC, ERR_INVALID_CONFIG_FILE,
		"");

	/* Get encryption configuration */
	ret = get_boot2_encrypt_type(&encrypt_type);
	sboot_assert((ret == 0), BOOT2_CLEANUP_FUNC, ret, "");

	initialize_encryption_context(&boot2_security, encrypt_type);

	/* Get signing configuration */
	ret = get_boot2_sign_type(&sign_type);
	sboot_assert((ret == 0), BOOT2_CLEANUP_FUNC, ret, "");

	initialize_signing_context(&boot2_security, sign_type);

	/* Return if nothing to do */
	if ((encrypt_type == NO_ENCRYPT) && (sign_type == NO_SIGN) &&
			(ks_len == sizeof(boot2_ks_hdr)) && !add_header &&
			(outdir == NULL)) {
		sboot_v("No encryption or signing was asked for boot2"
			" and no keystore is required");
		sboot_v("Doing nothing for boot2 image");
		return 0;
	}

	/* Get output file name */
	bootfile = get_outfile_name(infile, add_header,
			(ks_len > sizeof(boot2_ks_hdr)),
			encrypt_type, sign_type);
	sboot_assert(bootfile, BOOT2_CLEANUP_FUNC, ERR_BAD_OUTFILE_NAME, "");

	/* Get encryption parameters */
	if (encrypt_type != NO_ENCRYPT) {
		ret = tlv_store_get_ref(config_store, KEY_BOOT2_ENCRYPT_KEY,
					&encrypt_key_len, &encrypt_key);
		sboot_assert((ret >= 0), BOOT2_CLEANUP_FUNC, ERR_NO_ENCRYPT_KEY,
			"Boot2 encryption key is not specified");

		ret = tlv_store_get(config_store, KEY_BOOT2_NONCE,
					sizeof(iv), iv);
		sboot_assert((ret != ERR_TLV_BAD_BUF_LEN), BOOT2_CLEANUP_FUNC,
			ERR_NO_NONCE, "Boot2 iv/nonce is too long");

		if (ret == ERR_TLV_NOT_FOUND)
			/* Generate random nonce later*/
			iv_len = 0;
		else {
			/* AES HW engine requires iv length between 11 and 13
			 * for CCM*
			 */
			sboot_assert((ret >= 11 && ret <= 13),
				BOOT2_CLEANUP_FUNC, ERR_NO_NONCE,
				"Invalid length of boot2 iv/nonce");
			iv_len = ret;
		}
	}

	/* Get signing parameters */
	if (sign_type != NO_SIGN) {
		/* Get hash configuration */
		ret = get_boot2_hash_type(&hash_type);
		sboot_assert((ret == 0), BOOT2_CLEANUP_FUNC, ret, "");

		initialize_hash_context(&boot2_security, hash_type);

		ret = tlv_store_get_ref(config_store, KEY_BOOT2_PUBLIC_KEY,
					&pub_key_len, &pub_key_data);
		sboot_assert((ret >= 0), BOOT2_CLEANUP_FUNC, ERR_NO_PUBLIC_KEY,
			"Boot2 public key is not specified");

		ret = tlv_store_get_ref(config_store, KEY_BOOT2_PRIVATE_KEY,
					&sign_key_len, &sign_key_data);
		sboot_assert((ret >= 0), BOOT2_CLEANUP_FUNC, ERR_NO_PRIVATE_KEY,
			"Boot2 private key is not specified");
	}

	/* Get boot2 file data */
	in_len = file_read(infile, &buf);
	sboot_assert((in_len > 0), BOOT2_CLEANUP_FUNC, ERR_FILE_READ,
			"Could not read boot2 file %s: %s",
			infile, strerror(errno));

	/* Initialize soft crc16 algorithm */
	soft_crc16_init();

	/* Add bootrom header if raw boot2 file is specified */
	if (add_header) {
		ret = add_boot2_header(&buf, in_len);
		sboot_assert(ret > 0, BOOT2_CLEANUP_FUNC, ret,	"");
		in_len = ret;
	}

	/* Reallocate boot2 buffer to append keystore at the end */
	if (ks_len > sizeof(boot2_ks_hdr)) {
		in_buf = realloc(buf, in_len + ks_len + ALIGN_SIZE);
		sboot_assert(in_buf, BOOT2_CLEANUP_FUNC,
			ERR_INSUFFICIENT_MEMORY,
			"Failed to allocate memory for boot2 keystore");
	} else {
		in_buf = buf;
		ks_len = 0;
	}

	/* buf is either reallocated or assigned to in_buf */
	buf = NULL;

	/* Adjust boot2 header for keystore */
	es = (es_t *)in_buf;
	hdr = (hdr_t *) ((uint8_t *)es + sizeof(es_t));

	/* Sanity check. Otherwise codeLen can be junk */
	/* Check  magic code 'MRVL' */
	sboot_assert((hdr->bh.magicCode == 0x4D52564C), BOOT2_CLEANUP_FUNC,
		ERR_INVALID_BOOT2_FILE, "Invalid boot2 file %s", infile);

	/* Discard AES MIC and code sig */
	boot2 = (uint8_t *)hdr + sizeof(hdr_t);
	boot2_len = hdr->sh.codeLen;

	/* Skip bootrom header modification if no keystore */
	if (ks_len == 0)
		goto keystore_done;

	/* Boot2 expects keystore on 16-byte boundary. Hence
	 * align length to 16-bytes before keystore is appended. Fill void
	 * with 0xff.  Boot2 layout is:
	 *
	 * | es_t | hdr_t | boot2 code | alignmt padding |
	 *   keystore | alignmt padding | codeSig | mic |
	 */

	boot2_len = (boot2_len + sizeof(codeSig) + ALIGN_SIZE - 1)
						& (~(ALIGN_SIZE - 1));
	memset(boot2 + hdr->sh.codeLen, 0xff, boot2_len - hdr->sh.codeLen);

	/* Append keystore at the end */
	memcpy(boot2 + boot2_len, keystore, ks_len);
	boot2_len += ks_len;

	/* Align length to 16 byte boundary for AES-CCM. Fill void with 0xFF */
	aligned_length = boot2_len;
	aligned_length = (boot2_len + sizeof(codeSig) + ALIGN_SIZE - 1)
						& (~(ALIGN_SIZE - 1));
	aligned_length -= sizeof(codeSig);

	memset(boot2 + boot2_len, 0xff, aligned_length - boot2_len);
	boot2_len = aligned_length;

	codeSig = (uint16_t) soft_crc16(boot2, boot2_len, 0);
	memcpy(boot2 + boot2_len, &codeSig, sizeof(codeSig));

	/* Flash Section Header */
	hdr->sh.codeLen = boot2_len;
	hdr->sh.CRCCheck =  (uint16_t) soft_crc16(&hdr->sh,
				sizeof(hdr->sh) - sizeof(hdr->sh.CRCCheck), 0);

keystore_done:
	out_len = sizeof(hdr_t) + boot2_len + sizeof(codeSig);

	/* encryption part */
	if (encrypt_type != NO_ENCRYPT) {
		/* Encrypt the boot2 data */
		 if (iv_len == 0) {
			/* Generate random nonce if not specified */
			iv_len = boot2_security.encrypt_query(ENC_IV_LEN);
			ret = PlatformRandomBytes(iv, iv_len);
			sboot_assert((ret == 0),
					BOOT2_CLEANUP_FUNC, ERR_NO_NONCE,
					"Failed to generate random iv/nonce");
			sboot_v("Firmware iv/nonce len: %d", iv_len);
			sboot_v("Firmware iv/nonce: %s",
				bin2hex(iv, hex, iv_len, sizeof(hex)));
		}

		/* AES HW engine requires byte 13 set to 15 - iv_len for CCM* */
		for (ret = iv_len; ret < 16; ret++)
			iv[ret] = 0;
		iv[13] = 15 - iv_len;
		iv_len = 16;

		memcpy(es->nonce, iv, iv_len);

		ret = boot2_security.encrypt_init(&boot2_security, encrypt_key,
				 encrypt_key_len, iv, 15 - iv[13]);
		sboot_assert((ret >= 0), BOOT2_CLEANUP_FUNC, ERR_ENCRYPT_INIT,
			"Failed to initialize encryption algorithm: %d", ret);

		ret = boot2_security.encrypt(&boot2_security, hdr, out_len,
				(void **)&outbuf, (void **)&mic, &mic_len);
		sboot_assert((ret >= 0), BOOT2_CLEANUP_FUNC, ERR_ENCRYPT_DATA,
			"Failed to encrypt boot2: %d", ret);

		sboot_assert((mic_len <= sizeof(aes_mic)), BOOT2_CLEANUP_FUNC,
			ERR_ENCRYPT_DATA,
			"MIC length of boot2 is greater than %u bytes",
			(uint32_t) sizeof(aes_mic));

		if (mic_len) {
			sboot_v("Boot2 mic size: %d", mic_len);
			sboot_v("Boot2 mic: %s",
				bin2hex(mic, hex, mic_len, sizeof(hex)));
		}

		out_len = ret;
		es->encrypted_img_len = (uint32_t) out_len;
		memcpy(aes_mic, mic, mic_len);

		if (mic)
			free(mic);
		mic = NULL;
	} else
		outbuf = (uint8_t *)hdr;

	/* Signing part */
	if (sign_type != NO_SIGN) {
		es->encrypted_img_len = (uint32_t) out_len;
		memcpy(es->oem_pub_key, pub_key_data, pub_key_len);

		/* Calculate the hash of boot2 */
		ret = boot2_security.hash_init(&boot2_security);
		sboot_assert((ret >= 0), BOOT2_CLEANUP_FUNC, ERR_HASH_INIT,
			 "Failed to initialize hash algorithm: %d", ret);

		ret = boot2_security.hash_update(&boot2_security,
				outbuf, out_len);
		sboot_assert((ret >= 0), BOOT2_CLEANUP_FUNC, ERR_HASH_UPDATE,
			"Failed to update hash value of boot2: %d", ret);

		ret = boot2_security.hash_finish(&boot2_security,
				(void **)&hash);
		sboot_assert((ret >= 0), BOOT2_CLEANUP_FUNC, ERR_HASH_FINISH,
			"Failed to finalize hash value of boot2: %d", ret);

		hash_len = ret;

		sboot_v("Hash of Boot2: %s", bin2hex(hash, hex,
			hash_len, sizeof(hex)));

		/* Get the signature of hash of boot2 */
		ret = boot2_security.sign_init(&boot2_security,
				sign_key_data, sign_key_len);
		sboot_assert((ret >= 0), BOOT2_CLEANUP_FUNC, ERR_SIGNATURE_INIT,
			"Failed to initialize signature algorithm: %d", ret);

		ret = boot2_security.sign(&boot2_security, hash, hash_len,
				(void **)&sign);
		sboot_assert((ret >= 0), BOOT2_CLEANUP_FUNC,
			ERR_SIGNATURE_GENERATION,
			"Failed to sign the boot2: %d", ret);

		sboot_assert((ret <= sizeof(es->digital_sig)),
			BOOT2_CLEANUP_FUNC, ERR_SIGNATURE_GENERATION,
			"Boot2 signature is greater than %u bytes",
			(uint32_t) sizeof(es->digital_sig));

		sign_len = ret;
		memcpy(es->digital_sig, sign, sign_len);

		sboot_v("Boot2 signature size: %d", ret);
		sboot_v("Boot2 signature: %s",
			bin2hex(sign, hex, sign_len, sizeof(hex)));

		boot2_security.sign_deinit(&boot2_security);

		free(sign);
		free(hash);
		sign = hash = NULL;
	}

	/* Write header and data to output file */
	fp2 = fopen(bootfile, "wb");
	sboot_assert(fp2, BOOT2_CLEANUP_FUNC, ERR_FILE_OPEN,
		"Could not open secure boot2 file %s for write: %s",
		bootfile, strerror(errno));

	ret = fwrite(es, 1, sizeof(es_t), fp2);
	sboot_assert((ret == sizeof(es_t)), BOOT2_CLEANUP_FUNC, ERR_FILE_WRITE,
		"Could not write secure boot2 file %s: %s",
		bootfile, strerror(errno));

	ret = fwrite(outbuf, 1, (size_t) out_len, fp2);
	sboot_assert((ret == out_len), BOOT2_CLEANUP_FUNC, ERR_FILE_WRITE,
		"Could not write secure boot2 file %s: %s",
		bootfile, strerror(errno));

	ret = fwrite(aes_mic, 1, sizeof(aes_mic), fp2);
	sboot_assert((ret == sizeof(aes_mic)), BOOT2_CLEANUP_FUNC,
		ERR_FILE_WRITE, "Could not write secure boot2 file %s: %s",
		bootfile, strerror(errno));

	sboot_v("Created the secure boot2 image %s", bootfile);

	BOOT2_CLEANUP_FUNC;

	return 0;
}
