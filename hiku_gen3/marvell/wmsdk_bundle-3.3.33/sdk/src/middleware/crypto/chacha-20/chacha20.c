/*
 *  Copyright (C) 2014, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include "chacha20.h"

#define PLUS(v, w) (U32V((v) + (w)))
#define PLUSONE(v) (PLUS((v), 1))
void chacha20_init(chacha20_ctx_t *ctx, const uint8_t *key, uint32_t key_len,
	const uint8_t *iv, uint32_t iv_len)
{
	ECRYPT_init();
	ctx->cnt = 0;
	ECRYPT_keysetup((ECRYPT_ctx *)ctx, key, 8 * key_len,
		iv_len * 8);
	ECRYPT_ivsetup((ECRYPT_ctx *)ctx, iv);
}

void chacha20_encrypt(chacha20_ctx_t *ctx, const uint8_t *inputmsg,
		uint8_t *outputcipher, uint32_t bytes)
{
	ECRYPT_encrypt_bytes((ECRYPT_ctx *)ctx, inputmsg, outputcipher, bytes);
}

void chacha20_decrypt(chacha20_ctx_t *ctx, const uint8_t *inputcipher,
		uint8_t *outputmsg, uint32_t bytes)
{
	ECRYPT_decrypt_bytes((ECRYPT_ctx *)ctx, inputcipher, outputmsg, bytes);
}

void chacha20_increment_nonce(chacha20_ctx_t *ctx)
{
	ctx->input[14] = PLUSONE(ctx->input[14]);
	if (!ctx->input[14]) {
		ctx->input[15] = PLUSONE(ctx->input[15]);
	}
}
