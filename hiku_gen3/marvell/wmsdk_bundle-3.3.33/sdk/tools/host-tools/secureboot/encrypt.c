/*
 * Copyright (C) 2015, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <secure_boot.h>
#include "security.h"
#include "utils.h"

/* Update these lists appropriately whenever a new algorithm is added */
uint8_t boot2_encrypt_algos[] = {NO_ENCRYPT, AES_CCM_256_ENCRYPT};
uint8_t firmware_encrypt_algos[] = {
	NO_ENCRYPT,
	AES_CTR_128_ENCRYPT
};

uint8_t boot2_encrypt_algos_size = sizeof(boot2_encrypt_algos);
uint8_t firmware_encrypt_algos_size = sizeof(firmware_encrypt_algos);

/* AES CTR
 */

#define AES_CTR_CLUP1 aes_ctr_cleanup(security, NULL, NULL, NULL)
#define AES_CTR_CLUP2 aes_ctr_cleanup(security, outbuf, mic, mic_len)

void aes_ctr_cleanup(security_t *security, void **outbuf, void **mic,
		int *mic_len)
{
	if (security->encrypt_ctx) {
		OPENSSL_free(security->encrypt_ctx);
		security->encrypt_ctx = NULL;
	}

	if (outbuf && *outbuf) {
		OPENSSL_free(*outbuf);
		*outbuf = NULL;
	}

	if (mic && *mic && mic_len) {
		OPENSSL_free(*mic);
		*mic = NULL;
		*mic_len = 0;
	}
}

int aes_ctr_init(security_t *security, const EVP_CIPHER *cipher,
		const void *key, int key_len, const void *iv, int iv_len)
{
	EVP_CIPHER_CTX *ctx;
	int ret;

	EVP_add_cipher(cipher);
	ERR_load_crypto_strings();

	ctx = EVP_CIPHER_CTX_new();
	sboot_assert(ctx, , ERR_ENCRYPT_INIT, "%s",
		"Could not allocate memory for encrypt context");

	security->encrypt_ctx = ctx;

	ret = EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL);
	sboot_assert((ret == 1), AES_CTR_CLUP1, ERR_ENCRYPT_INIT, "%s",
		ERR_error_string(ERR_get_error(), NULL));

	sboot_assert(iv_len == EVP_CIPHER_CTX_iv_length(ctx), ,
		ERR_ENCRYPT_INIT,
		"IV should be of size %d bytes", EVP_CIPHER_CTX_iv_length(ctx));

	ret = EVP_CIPHER_CTX_set_key_length(ctx, key_len);
	sboot_assert((ret == 1), AES_CTR_CLUP1, ERR_ENCRYPT_INIT, "%s",
		ERR_error_string(ERR_get_error(), NULL));

	ret = EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv);
	sboot_assert((ret == 1), AES_CTR_CLUP1, ERR_ENCRYPT_INIT, "%s",
		ERR_error_string(ERR_get_error(), NULL));

	return 0;
}

int aes_ctr_query(const EVP_CIPHER *cipher, query_t query)
{
	switch (query) {
	case ENC_IV_LEN:
		return EVP_CIPHER_iv_length(cipher);
	default:
		break;
	}

	return ERR_ENCRYPT_QUERY;
}

int aes_ctr_encrypt(security_t *security, void *inbuf, int inlen,
		void **outbuf, void **mic, int *mic_len)
{
	EVP_CIPHER_CTX *ctx = (EVP_CIPHER_CTX *)security->encrypt_ctx;
	int ret, outlen, len;

	sboot_assert(ctx, , ERR_ENCRYPT_DATA, "%s",
		"Encrypt Initialization Routine was not called");

#if 0
	/* Why this does not work for CTR? */
	ret = EVP_EncryptUpdate(ctx, NULL, &outlen, NULL, inlen);
	sboot_assert((ret == 1), , ERR_ENCRYPT_DATA, "%s",
		ERR_error_string(ERR_get_error(), NULL));
#else
	outlen = inlen;
#endif
	*mic = NULL;
	*mic_len = 0;

	*outbuf = OPENSSL_malloc(outlen);
	sboot_assert(*outbuf, AES_CTR_CLUP2, ERR_ENCRYPT_DATA,
		"Could not allocate memory for encrypted buffer");

	ret = EVP_EncryptUpdate(ctx, *outbuf, &outlen, inbuf, inlen);
	sboot_assert((ret == 1), AES_CTR_CLUP2, ERR_ENCRYPT_DATA, "%s",
		ERR_error_string(ERR_get_error(), NULL));

	ret = EVP_EncryptFinal_ex(ctx, *outbuf + outlen, &len);
	sboot_assert((ret == 1), AES_CTR_CLUP2, ERR_ENCRYPT_DATA, "%s",
		ERR_error_string(ERR_get_error(), NULL));
	outlen += len;

	EVP_CIPHER_CTX_free(ctx);
	security->encrypt_ctx = NULL;

	return outlen;
}

int aes_ctr_128_init(security_t *security, const void *key, int key_len,
		const void *iv, int iv_len)
{
	return aes_ctr_init(security, EVP_aes_128_ctr(), key, key_len,
			iv, iv_len);
}

int aes_ctr_128_query(query_t query)
{
	return aes_ctr_query(EVP_aes_128_ctr(), query);
}

/* AES CCM
 */

#define AES_CCM_CLUP1 AES_CTR_CLUP1
#define AES_CCM_CLUP2 AES_CTR_CLUP2

int aes_ccm_init(security_t *security, const EVP_CIPHER *cipher,
		const void *key, int key_len, const void *iv, int iv_len)
{
	EVP_CIPHER_CTX *ctx;
	int ret;

	EVP_add_cipher(cipher);
	ERR_load_crypto_strings();

	ctx = EVP_CIPHER_CTX_new();
	sboot_assert(ctx, , ERR_ENCRYPT_INIT, "%s",
		"Could not allocate memory for encrypt context");

	security->encrypt_ctx = ctx;

	ret = EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL);
	sboot_assert((ret == 1), AES_CCM_CLUP1, ERR_ENCRYPT_INIT, "%s",
		ERR_error_string(ERR_get_error(), NULL));

	ret = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_SET_IVLEN,
		iv_len, NULL);
	sboot_assert((ret == 1), AES_CCM_CLUP1, ERR_ENCRYPT_INIT, "%s",
		ERR_error_string(ERR_get_error(), NULL));

	ret = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_SET_TAG, 16, NULL);
	sboot_assert((ret == 1), AES_CCM_CLUP1, ERR_ENCRYPT_INIT, "%s",
		ERR_error_string(ERR_get_error(), NULL));

	ret = EVP_CIPHER_CTX_set_key_length(ctx, key_len);
	sboot_assert((ret == 1), AES_CCM_CLUP1, ERR_ENCRYPT_INIT, "%s",
		ERR_error_string(ERR_get_error(), NULL));

	ret = EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv);
	sboot_assert((ret == 1), AES_CCM_CLUP1, ERR_ENCRYPT_INIT, "%s",
		ERR_error_string(ERR_get_error(), NULL));

	return 0;
}

int aes_ccm_query(const EVP_CIPHER *cipher, query_t query)
{
	switch (query) {
	case ENC_IV_LEN:
		return EVP_CIPHER_iv_length(cipher);
	default:
		break;
	}

	return ERR_ENCRYPT_QUERY;
}

int aes_ccm_encrypt(security_t *security, void *inbuf, int inlen,
		void **outbuf, void **mic, int *mic_len)
{
	EVP_CIPHER_CTX *ctx = (EVP_CIPHER_CTX *)security->encrypt_ctx;
	int ret, outlen, len;

	sboot_assert(ctx, , ERR_ENCRYPT_DATA, "%s",
		"Encrypt Initialization Routine was not called");

	*outbuf = NULL;
	*mic = NULL;
	*mic_len = 0;

	ret = EVP_EncryptUpdate(ctx, NULL, &outlen, NULL, inlen);
	sboot_assert((ret == 1), AES_CCM_CLUP2, ERR_ENCRYPT_DATA, "%s",
		ERR_error_string(ERR_get_error(), NULL));

	*outbuf = OPENSSL_malloc(outlen);
	sboot_assert(*outbuf, AES_CCM_CLUP2, ERR_ENCRYPT_DATA,
		"Could not allocate memory for encrypted buffer");

	ret = EVP_EncryptUpdate(ctx, *outbuf, &outlen, inbuf, inlen);
	sboot_assert((ret == 1), AES_CCM_CLUP2, ERR_ENCRYPT_DATA, "%s",
		ERR_error_string(ERR_get_error(), NULL));

	ret = EVP_EncryptFinal_ex(ctx, *outbuf + outlen, &len);
	sboot_assert((ret == 1), AES_CCM_CLUP2, ERR_ENCRYPT_DATA, "%s",
		ERR_error_string(ERR_get_error(), NULL));
	outlen += len;

	/* How to get mic length? */
	*mic = OPENSSL_malloc(16);
	sboot_assert(*mic, AES_CCM_CLUP2, ERR_ENCRYPT_DATA,
		"Could not allocate memory for mic");

	ret = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_GET_TAG, 16, *mic);
	sboot_assert((ret == 1), AES_CCM_CLUP2, ERR_ENCRYPT_DATA, "%s",
		ERR_error_string(ERR_get_error(), NULL));
	*mic_len = 16;

	EVP_CIPHER_CTX_free(ctx);
	security->encrypt_ctx = NULL;

	return outlen;
}

int aes_ccm_256_init(security_t *security, const void *key, int key_len,
		const void *iv, int iv_len)
{
	return aes_ccm_init(security, EVP_aes_256_ccm(), key, key_len,
			iv, iv_len);
}

int aes_ccm_256_query(query_t query)
{
	return aes_ccm_query(EVP_aes_256_ccm(), query);
}

/* Update this function when new algorithm is added */
int initialize_encryption_context(security_t *security, uint8_t encrypt_type)
{
	security->encrypt_ctx = NULL;
	security->iv = NULL;
	security->iv_len = 0;

	switch (encrypt_type) {
	case AES_CTR_128_ENCRYPT:
		security->encrypt_init = aes_ctr_128_init;
		security->encrypt_query = aes_ctr_128_query;
		security->encrypt = aes_ctr_encrypt;
		break;
	case AES_CCM_256_ENCRYPT:
		security->encrypt_init = aes_ccm_256_init;
		security->encrypt_query = aes_ccm_256_query;
		security->encrypt = aes_ccm_encrypt;
		break;
	default:
		security->encrypt_init = NULL;
		security->encrypt_query = NULL;
		security->encrypt = NULL;
		encrypt_type = NO_ENCRYPT;
		break;
	}

	return encrypt_type;
}

