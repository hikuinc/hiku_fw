/* rabbit.h
 *
 * Copyright (C) 2006-2015 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of wolfSSL.
 *
 * Contact licensing@wolfssl.com with any questions or comments.
 *
 * http://www.wolfssl.com
 */


#ifndef WOLF_CRYPT_RABBIT_H
#define WOLF_CRYPT_RABBIT_H

#include <wolfssl/wolfcrypt/types.h>

#ifndef NO_RABBIT

#ifdef __cplusplus
    extern "C" {
#endif


enum {
	RABBIT_ENC_TYPE  = 5     /* cipher unique type */
};


/* Rabbit Context */
typedef struct RabbitCtx {
    word32 x[8];
    word32 c[8];
    word32 carry;
} RabbitCtx;


/* Rabbit stream cipher */
typedef struct Rabbit {
    RabbitCtx masterCtx;
    RabbitCtx workCtx;
} Rabbit;


WOLFSSL_API int wc_RabbitProcess(Rabbit*, byte*, const byte*, word32);
WOLFSSL_API int wc_RabbitSetKey(Rabbit*, const byte* key, const byte* iv);


#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* NO_RABBIT */
#endif /* WOLF_CRYPT_RABBIT_H */

