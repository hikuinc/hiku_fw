/*
 * Copyright (C) 2008-2015, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <rfget.h>
#include <fw_upgrade_rsa_aes.h>
#include <secure_upgrade.h>
#include <hkdf-sha.h>

#include <wolfcrypt/settings.h>
#include <ctaocrypt/settings_comp.h>
#include <cyassl/ctaocrypt/rsa.h>
#include <cyassl/ctaocrypt/aes.h>

#define RSA_SIGN_LEN 256
#define SHA512_HASH_SIZE 64

#define fwupg_l(...)                              \
		wmlog("fwupg", ##__VA_ARGS__)

#define fwupg_e(...)                              \
		wmlog_e("fwupg", ##__VA_ARGS__)

static SHA512Context sha;
static RsaKey key;
static word32 idx;
static Aes dec;
static uint8_t sig[RSA_SIGN_LEN];
static uint8_t hash[SHA512_HASH_SIZE];
static uint8_t nonce[AES_BLOCK_SIZE];

/* Initialize the AES CTR 128 context.
 */
static void decrypt_init_func(void *context, void *key, void *iv)
{

	wc_AesSetKeyCtr(context, key, AES_BLOCK_SIZE,
			iv, AES_ENCRYPTION);
}

/* This function needs to be defined to match the signature of decrypt_func_t
 * Use appropriate decryption function in it.
 */
static int decrypt_func(void *context, void *inbuf, int inlen,
			void *outbuf, int outlen)
{
	if (outlen - inlen)
		return -WM_FAIL;

	AesCtrEncrypt(&dec, outbuf, inbuf, inlen);

	return inlen;
}

/* This function needs to be defined to match the signature of
 * sign_verify_func_t. Use appropriate sign verification function in it.
 */
static int signature_verify_func(void *message, int len, void *pk,
				 void *signature)
{
	int ret;
	uint8_t hashDecrypt[SHA512_HASH_SIZE];

	ret = RsaSSL_Verify(signature, RSA_SIGN_LEN, hashDecrypt,
			    sizeof(hashDecrypt), pk);

	/* Compare if the SHA512 Hash of the decrypted firmware image
	   matches with SHA512 hash derived from Signature */
	if (memcmp(message, hashDecrypt, sizeof(hashDecrypt)) || ret < 0) {
		return -1;
	}

	return 0;
}

/* This function needs to be defined to match the signature of
 * hash_update_func_t. Use appropriate hash update function in it.
 */
static void hash_update_func(void *context, void *buf, int len)
{
	SHA512Input(context, buf, len);
}

/* This function needs to be defined to match the signature of
 * hash_finish_func_t. Use appropriate hash finish function in it.
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
	.decrypt_ctx = &dec,
	.hash_ctx = &sha,
	.signature = &sig,
	.signature_len = RSA_SIGN_LEN,
	.hash = &hash,
	.hash_len = SHA512_HASH_SIZE,
	.pk = &key,
	.iv = &nonce,
	.iv_len = AES_BLOCK_SIZE,
};


/* This function will initialize the hashing context appropriately and add
 * the verfication_pk and decrypt_key to the structure.
 * It will then start the secure firmware upgrade.
 * It must be called after the URL of the firmware image location is available.
 */
int rsa_aes_client_mode_upgrade_fw(enum flash_comp flash_comp_type,
					  char *fw_upgrade_url,
					  uint8_t *verification_pk,
					  uint8_t *decrypt_key,
					  const tls_client_t *cfg)
{
	int ret = 0;
	fwupg_l("Upgrading firmware...");

	ctaocrypt_lib_init();
	idx = 0;

	SHA512Reset(&sha);
	InitRsaKey(&key, 0);

	ret = RsaPublicKeyDecode(verification_pk, &idx, &key,
				 RSA_PUBLIC_KEY_LEN);
	if (ret != 0) {
		rf_e("RSA Public ket decode error : %d", ret);
		return -WM_FAIL;
	}

	priv_data.decrypt_key = decrypt_key;
	priv_data.fw_data_read_fn = NULL;
	priv_data.session_priv_data = NULL;
	ret = client_mode_secure_upgrade(flash_comp_type, fw_upgrade_url,
					 &priv_data, cfg);
	if (ret != WM_SUCCESS)
		fwupg_e("Firmware upgrade failed!");
	else
		fwupg_l("Firmware upgrade successful!");
	FreeRsaKey(&key);
	return ret;
}


/* This function will initialize the hashing context appropriately and add
 * the verfication_pk and decrypt_key to the structure.
 * It will also set the fw_data_read_fn.
 * It will then start the secure firmware upgrade.
 * It must be called after the URL of the firmware image location is available.
 */
int rsa_aes_server_mode_upgrade_fw(enum flash_comp flash_comp_type,
					  uint8_t *verification_pk,
					  uint8_t *decrypt_key,
					  fw_data_read_fn_t fw_data_read_fn,
					  void *priv)
{
	int ret;
	fwupg_l("Upgrading %sfirmware...",
		flash_comp_type == FC_COMP_WLAN_FW ? "Wi-Fi " : "");

	ctaocrypt_lib_init();
	idx = 0;

	SHA512Reset(&sha);
	InitRsaKey(&key, 0);

	ret = RsaPublicKeyDecode(verification_pk, &idx, &key,
				 RSA_PUBLIC_KEY_LEN);
	if (ret != 0) {
		rf_e("RSA Public ket decode error : %d", ret);
		return -WM_FAIL;
	}

	priv_data.decrypt_key = decrypt_key;
	priv_data.fw_data_read_fn = fw_data_read_fn;
	priv_data.session_priv_data = priv;
	ret = server_mode_secure_upgrade(flash_comp_type, &priv_data);
	if (ret != WM_SUCCESS)
		fwupg_e("Firmware upgrade failed!");
	else
		fwupg_l("Firmware upgrade successful!");
	FreeRsaKey(&key);
	return ret;
}
