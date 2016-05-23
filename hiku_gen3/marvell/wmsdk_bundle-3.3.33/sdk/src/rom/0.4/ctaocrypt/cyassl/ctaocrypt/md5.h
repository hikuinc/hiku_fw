/* md5.h
 *
 * Copyright (C) 2006-2014 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@wolfssl.com with any questions or comments.
 *
 * http://www.wolfssl.com
 */


#ifndef NO_MD5

#ifndef CTAO_CRYPT_MD5_H
#define CTAO_CRYPT_MD5_H

#include <cyassl/ctaocrypt/types.h>

#ifdef __cplusplus
    extern "C" {
#endif


/* in bytes */
enum {
#ifdef STM32F2_HASH
    MD5_REG_SIZE    =  4,      /* STM32 register size, bytes */
#endif
    MD5             =  0,      /* hash type unique */
    MD5_BLOCK_SIZE  = 64,
    MD5_DIGEST_SIZE = 16,
    MD5_PAD_SIZE    = 56
};

#ifdef CYASSL_PIC32MZ_HASH
#include "port/pic32/pic32mz-crypt.h"
#endif

/* MD5 digest */
typedef struct Md5 {
    word32  buffLen;   /* in bytes          */
    word32  loLen;     /* length in bytes   */
    word32  hiLen;     /* length in bytes   */
    word32  buffer[MD5_BLOCK_SIZE  / sizeof(word32)];
    #ifndef CYASSL_PIC32MZ_HASH
    word32  digest[MD5_DIGEST_SIZE / sizeof(word32)];
    #else
    word32  digest[PIC32_HASH_SIZE / sizeof(word32)];
    pic32mz_desc desc ; /* Crypt Engine descripter */
    #endif
} Md5;

CYASSL_API void InitMd5(Md5*);
CYASSL_API void Md5Update(Md5*, const byte*, word32);
CYASSL_API void Md5Final(Md5*, byte*);
CYASSL_API int  Md5Hash(const byte*, word32, byte*);


#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* CTAO_CRYPT_MD5_H */
#endif /* NO_MD5 */
