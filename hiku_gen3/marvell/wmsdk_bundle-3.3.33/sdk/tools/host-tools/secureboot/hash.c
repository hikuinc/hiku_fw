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
uint8_t boot2_hash_algos[] = {NO_HASH, SHA256_HASH};
uint8_t firmware_hash_algos[] = {NO_HASH, SHA256_HASH};

uint8_t boot2_hash_algos_size = sizeof(boot2_hash_algos);
uint8_t firmware_hash_algos_size = sizeof(firmware_hash_algos);

/* SHA 256
 */

#define SHA256_CLUP1 sha256_cleanup(security, NULL)
#define SHA256_CLUP2 sha256_cleanup(security, result)

static void sha256_cleanup(security_t *security, void **result)
{
	if (security->hash_ctx) {
		EVP_MD_CTX_destroy((EVP_MD_CTX *)security->hash_ctx);
		security->hash_ctx = NULL;
	}
	if (result && *result) {
		OPENSSL_free(*result);
		*result = NULL;
	}
}

int sha256_init(security_t *security)
{
	int ret;
	EVP_MD_CTX *mdctx = EVP_MD_CTX_create();
	sboot_assert(mdctx, SHA256_CLUP1, ERR_HASH_INIT,
		"Could not allocate memory for hash context");

	security->hash_ctx = mdctx;

	EVP_add_digest(EVP_sha256());
	ERR_load_crypto_strings();

	sboot_assert(mdctx, SHA256_CLUP1, ERR_HASH_INIT, "%s",
		ERR_error_string(ERR_get_error(), NULL));

	ret = EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
	sboot_assert((ret == 1), SHA256_CLUP1, ERR_HASH_INIT, "%s",
		ERR_error_string(ERR_get_error(), NULL));
	return 0;
}

int sha256_update(security_t *security, const void *buf, int buflen)
{
	EVP_MD_CTX *mdctx = (EVP_MD_CTX *)security->hash_ctx;
	int ret;

	sboot_assert(mdctx, , ERR_HASH_UPDATE, "%s",
		"Hash Initialization Routine was not called");

	ret = EVP_DigestUpdate(mdctx,  buf, buflen);
	sboot_assert((ret == 1), SHA256_CLUP1, ERR_HASH_UPDATE, "%s",
		ERR_error_string(ERR_get_error(), NULL));

	return 0;
}

int sha256_finish(security_t *security, void **result)
{
	int ret;
	unsigned int md_len;
	EVP_MD_CTX *mdctx = (EVP_MD_CTX *)security->hash_ctx;

	sboot_assert(mdctx, , ERR_HASH_UPDATE, "%s",
		"Hash Initialization Routine was not called");

	*result = OPENSSL_malloc(EVP_MAX_MD_SIZE);
	sboot_assert(*result, SHA256_CLUP2, ERR_HASH_FINISH,
		"Could not allocate memory for hash value");

	ret = EVP_DigestFinal_ex(mdctx, *result, &md_len);
	sboot_assert((ret == 1), SHA256_CLUP2, ERR_HASH_FINISH, "%s",
		ERR_error_string(ERR_get_error(), NULL));

	EVP_MD_CTX_destroy(mdctx);
	security->hash_ctx = NULL;

	return md_len;
}

/* Update this function when new algorithm is added */
int initialize_hash_context(security_t *security, uint8_t hash_type)
{
	security->hash_ctx = NULL;

	switch (hash_type) {
	case SHA256_HASH:
		security->hash_init = sha256_init;
		security->hash_update = sha256_update;
		security->hash_finish = sha256_finish;
		break;
	default:
		security->hash_init = NULL;
		security->hash_update = NULL;
		security->hash_finish = NULL;
		hash_type = NO_HASH;
		break;
	}

	return hash_type;
}
