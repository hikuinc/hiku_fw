/*
 * Copyright (C) 2014, Marvell International Ltd.
 * All Rights Reserved.
 */
#include <fw_upgrade_ed_chacha.h>
#include <secure_upgrade.h>
#include <chacha20.h>
#include <ed25519.h>
#include <hkdf-sha.h>

#define CHACHA_NONCE_LEN	8

#define fwupg_l(...)                              \
		wmlog("fwupg", ##__VA_ARGS__)

#define fwupg_e(...)                              \
		wmlog_e("fwupg", ##__VA_ARGS__)

static chacha20_ctx_t chctx;
static SHA512Context     sha;
static uint8_t hash[64];
static ed25519_signature ed_sign;
static uint8_t nonce[CHACHA_NONCE_LEN];

/* Initialize the Chacha20 context.
 */
static void decrypt_init_func(void *context, void *key, void *iv)
{
	chacha20_init(context, key, ED_CHACHA_DECRYPT_KEY_LEN, iv,
			CHACHA_NONCE_LEN);
}
/* This function needs to be defined to match the signature of decrypt_func_t
 * We will use chacha20 for decryption.
 */
static int decrypt_func(void *context, void *inbuf, int inlen,
		void *outbuf, int outlen)
{
	if (outlen < inlen)
		return -WM_FAIL;
	chacha20_decrypt(context, inbuf, outbuf, inlen);
	return inlen;
}

/* This function needs to be defined to match the signature of
 * sign_verify_func_t. We will use ED25519 for verification.
 */
static int signature_verify_func(void *message, int len, void *pk,
		void *signature)
{
	return ed25519_sign_open(message, len, pk, signature);
}

/* This function needs to be defined to match the signature of
 * hash_update_func_t. We will use SHA512
 */
static void hash_update_func(void *context, void *buf, int len)
{
	SHA512Input(context, buf, len);
}

/* This function needs to be defined to match the signature of
 * hash_finish_func_t. We will use SHA512.
 */
static void hash_finish_func(void *context, void *result)
{
	SHA512Result(context, result);
}

/* Define the upgrade structure. Initialise all function pointers as
 * above. Initialise the other parameters as per the encryption,
 * signing and hashing algorithms. Please use global variables for
 * data pointers. The memory should remain allocated throughout
 * the lifetime of the upgrade process.
 */
static upgrade_struct_t priv_data = {
	.decrypt_init = decrypt_init_func,
	.decrypt = decrypt_func,
	.signature_verify = signature_verify_func,
	.hash_update = hash_update_func,
	.hash_finish = hash_finish_func,
	.decrypt_ctx = &chctx,
	.hash_ctx = &sha,
	.iv = &nonce,
	.iv_len = 8,
	.signature = &ed_sign,
	.signature_len = 64,
	.hash = &hash,
	.hash_len = 64,
};

/* This function will initialize the hashing context appropriately and add
 * the verfication_pk and decrypt_key to the structure.
 * It will also set the fw_data_read_fn.
 * It will then start the secure firmware upgrade.
 * It must be called after the URL of the firmware image location is available.
 */
int ed_chacha_server_mode_upgrade_fw(enum flash_comp flash_comp_type,
		uint8_t *verification_pk, uint8_t *decrypt_key,
		fw_data_read_fn_t fw_data_read_fn, void *priv)
{
	int ret;
	fwupg_l("Upgrading %sfirmware...",
			flash_comp_type == FC_COMP_WLAN_FW ? "Wi-Fi " : "");
	SHA512Reset(priv_data.hash_ctx);
	priv_data.pk = verification_pk;
	priv_data.decrypt_key = decrypt_key;
	priv_data.fw_data_read_fn = fw_data_read_fn;
	priv_data.session_priv_data = priv;
	ret = server_mode_secure_upgrade(flash_comp_type, &priv_data);
	if (ret != WM_SUCCESS)
		fwupg_e("Firmware upgrade failed!");
	else
		fwupg_l("Firmware upgrade successful!");
	return ret;
}
/* This function will initialize the hashing context appropriately and add
 * the verfication_pk and decrypt_key to the structure.
 * It will then start the secure firmware upgrade.
 * It must be called after the URL of the firmware image location is available.
 */
int ed_chacha_client_mode_upgrade_fw(enum flash_comp flash_comp_type,
		char *fw_upgrade_url, uint8_t *verification_pk,
		uint8_t *decrypt_key, tls_client_t *cfg)
{
	int ret;
	fwupg_l("Upgrading firmware...");
	SHA512Reset(priv_data.hash_ctx);
	priv_data.pk = verification_pk;
	priv_data.decrypt_key = decrypt_key;
	priv_data.fw_data_read_fn = NULL;
	priv_data.session_priv_data = NULL;
	ret = client_mode_secure_upgrade(flash_comp_type, fw_upgrade_url,
			&priv_data, cfg);
	if (ret != WM_SUCCESS)
		fwupg_e("Firmware upgrade failed!");
	else
		fwupg_l("Firmware upgrade successful!");
	return ret;
}
