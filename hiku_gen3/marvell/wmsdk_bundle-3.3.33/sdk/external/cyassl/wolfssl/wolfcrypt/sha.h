/* sha.h
 *
 * Copyright (C) 2006-2015 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of wolfSSL.
 *
 * Contact licensing@wolfssl.com with any questions or comments.
 *
 * http://www.wolfssl.com
 */


#ifndef WOLF_CRYPT_SHA_H
#define WOLF_CRYPT_SHA_H

#include <wolfssl/wolfcrypt/types.h>

#ifndef NO_SHA

#ifdef HAVE_FIPS
/* for fips @wc_fips */
#include <cyassl/ctaocrypt/sha.h>
#endif

#ifdef __cplusplus
    extern "C" {
#endif

#ifndef HAVE_FIPS /* avoid redefining structs */
/* in bytes */
enum {
#ifdef STM32F2_HASH
    SHA_REG_SIZE     =  4,    /* STM32 register size, bytes */
#endif
    SHA              =  1,    /* hash type unique */
    SHA_BLOCK_SIZE   = 64,
    SHA_DIGEST_SIZE  = 20,
    SHA_PAD_SIZE     = 56
};

#ifdef WOLFSSL_PIC32MZ_HASH
#include "port/pic32/pic32mz-crypt.h"
#endif

#ifndef WOLFSSL_TI_HASH
      
/* Sha digest */
typedef struct Sha {
    word32  buffLen;   /* in bytes          */
    word32  loLen;     /* length in bytes   */
    word32  hiLen;     /* length in bytes   */
    word32  buffer[SHA_BLOCK_SIZE  / sizeof(word32)];
    #ifndef WOLFSSL_PIC32MZ_HASH
        word32  digest[SHA_DIGEST_SIZE / sizeof(word32)];
    #else
        word32  digest[PIC32_HASH_SIZE / sizeof(word32)];
        pic32mz_desc desc; /* Crypt Engine descripter */
    #endif
} Sha;

#else /* WOLFSSL_TI_HASH */
    #include "wolfssl/wolfcrypt/port/ti/ti-hash.h"
#endif

#endif /* HAVE_FIPS */

WOLFSSL_API int wc_InitSha(Sha*);
WOLFSSL_API int wc_ShaUpdate(Sha*, const byte*, word32);
WOLFSSL_API int wc_ShaFinal(Sha*, byte*);

#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* NO_SHA */
#endif /* WOLF_CRYPT_SHA_H */

