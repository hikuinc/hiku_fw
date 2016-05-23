/*
 * Copyright (C) 2015, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <stdint.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <secure_boot.h>
#include "security.h"
#include "utils.h"

/* Update these lists appropriately whenever a new algorithm is added */
uint8_t boot2_signing_algos[] = {NO_SIGN, RSA_SIGN};
uint8_t firmware_signing_algos[] = {NO_SIGN, RSA_SIGN};

uint8_t boot2_signing_algos_size = sizeof(boot2_signing_algos);
uint8_t firmware_signing_algos_size = sizeof(firmware_signing_algos);

/* RSA Sign
 */

int rsa_init(security_t *security, const void *private_key, int key_len)
{
	EVP_PKEY_CTX *pkctx = NULL;
	EVP_PKEY *pkey = NULL;
	int ret = 0;

	ERR_load_crypto_strings();

	pkey = d2i_PrivateKey(EVP_PKEY_RSA, NULL,
		(const unsigned char **)&private_key, key_len);
	sboot_assert(pkey, , ERR_SIGNATURE_INIT, "%s",
		ERR_error_string(ERR_get_error(), NULL));

	pkctx = EVP_PKEY_CTX_new(pkey, NULL);
	sboot_assert(pkctx, EVP_PKEY_free(pkey), ERR_SIGNATURE_INIT, "%s",
		ERR_error_string(ERR_get_error(), NULL));

	EVP_PKEY_free(pkey);

	/* Initialise the DigestSign operation */
	ret = EVP_PKEY_sign_init(pkctx);
	sboot_assert((ret == 1), EVP_PKEY_CTX_free(pkctx), ERR_SIGNATURE_INIT,
		"%s", ERR_error_string(ERR_get_error(), NULL));

	security->sign_ctx = pkctx;
	return 0;
}

#define RSA_CLEANUP_FUNC rsa_cleanup(security, sign)
void rsa_deinit(security_t *security);

void rsa_cleanup(security_t *security, void **sign)
{
	if (*sign) {
		OPENSSL_free(*sign);
		*sign = NULL;
	}

	rsa_deinit(security);
}

int rsa_sign(security_t *security, const void *message, int len, void **sign)
{
	EVP_PKEY_CTX *pkctx = security->sign_ctx;
	int ret;
	size_t slen;

	*sign = NULL;
	sboot_assert(pkctx, RSA_CLEANUP_FUNC, ERR_SIGNATURE_GENERATION,
		"Signature Initialization Routine was not called");

	/* First call EVP_PKEY_sign with a NULL sig parameter to obtain the
	 * length of the signature. Length is returned in slen */
	ret = EVP_PKEY_sign(pkctx, NULL, &slen, message, len);
	sboot_assert((ret == 1), RSA_CLEANUP_FUNC, ERR_SIGNATURE_GENERATION,
		"%s", ERR_error_string(ERR_get_error(), NULL));

	/* Allocate memory for the signature based on size in slen */
	*sign = OPENSSL_malloc(sizeof(unsigned char) * slen);
	sboot_assert(*sign, RSA_CLEANUP_FUNC, ERR_SIGNATURE_GENERATION,
		"Could not allocate memory for signature buffer");

	/* Obtain the signature */
	ret = EVP_PKEY_sign(pkctx, *sign, &slen, message, len);
	sboot_assert((ret == 1), RSA_CLEANUP_FUNC, ERR_SIGNATURE_GENERATION,
		"%s", ERR_error_string(ERR_get_error(), NULL));

	return slen;
}

void rsa_deinit(security_t *security)
{
	EVP_PKEY_CTX *pkctx = security->sign_ctx;

	if (pkctx) {
		EVP_PKEY_CTX_free(pkctx);
		security->sign_ctx = NULL;
	}
}

/* Update this function when new algorithm is added */
int initialize_signing_context(security_t *security, uint8_t signing_type)
{
	security->sign_ctx = NULL;

	switch (signing_type) {
	case RSA_SIGN:
		security->sign_init = rsa_init;
		security->sign = rsa_sign;
		security->sign_deinit = rsa_deinit;
		break;
	default:
		security->sign_init = NULL;
		security->sign = NULL;
		security->sign_deinit = NULL;
		signing_type = NO_SIGN;
		break;
	}

	return signing_type;
}
