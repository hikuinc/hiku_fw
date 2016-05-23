/* sha.h
 *
 * Copyright (C) 2006-2014 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@wolfssl.com with any questions or comments.
 *
 * http://www.wolfssl.com
 */



#ifndef NO_SHA

#ifndef CTAO_CRYPT_SHA_H
#define CTAO_CRYPT_SHA_H

#include <cyassl/ctaocrypt/types.h>

#ifdef __cplusplus
    extern "C" {
#endif


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

#ifdef CYASSL_PIC32MZ_HASH
#include "port/pic32/pic32mz-crypt.h"
#endif

/* Sha digest */
typedef struct Sha {
    word32  buffLen;   /* in bytes          */
    word32  loLen;     /* length in bytes   */
    word32  hiLen;     /* length in bytes   */
    word32  buffer[SHA_BLOCK_SIZE  / sizeof(word32)];
    #ifndef CYASSL_PIC32MZ_HASH
        word32  digest[SHA_DIGEST_SIZE / sizeof(word32)];
    #else
        word32  digest[PIC32_HASH_SIZE / sizeof(word32)];
        pic32mz_desc desc; /* Crypt Engine descripter */
    #endif
} Sha;


CYASSL_API int InitSha(Sha*);
CYASSL_API int ShaUpdate(Sha*, const byte*, word32);
CYASSL_API int ShaFinal(Sha*, byte*);
CYASSL_API int ShaHash(const byte*, word32, byte*);


#ifdef HAVE_FIPS
    /* fips wrapper calls, user can call direct */
    CYASSL_API int InitSha_fips(Sha*);
    CYASSL_API int ShaUpdate_fips(Sha*, const byte*, word32);
    CYASSL_API int ShaFinal_fips(Sha*, byte*);
    #ifndef FIPS_NO_WRAPPERS
        /* if not impl or fips.c impl wrapper force fips calls if fips build */
        #define InitSha   InitSha_fips
        #define ShaUpdate ShaUpdate_fips
        #define ShaFinal  ShaFinal_fips
    #endif /* FIPS_NO_WRAPPERS */

#endif /* HAVE_FIPS */

 
#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* CTAO_CRYPT_SHA_H */
#endif /* NO_SHA */

