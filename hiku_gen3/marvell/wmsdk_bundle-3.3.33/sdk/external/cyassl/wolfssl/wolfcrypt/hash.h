/* hash.h
 *
 * Copyright (C) 2006-2015 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of wolfSSL.
 *
 * Contact licensing@wolfssl.com with any questions or comments.
 *
 * http://www.wolfssl.com
 */


#ifndef WOLF_CRYPT_HASH_H
#define WOLF_CRYPT_HASH_H

#include <wolfssl/wolfcrypt/types.h>

#ifdef __cplusplus
    extern "C" {
#endif

/* Hash types */
enum wc_HashType {
    WC_HASH_TYPE_NONE = 0,
#ifdef WOLFSSL_MD2
    WC_HASH_TYPE_MD2 = 1,
#endif
#ifndef NO_MD4
    WC_HASH_TYPE_MD4 = 2,
#endif
#ifndef NO_MD5
    WC_HASH_TYPE_MD5 = 3,
#endif
#ifndef NO_SHA
    WC_HASH_TYPE_SHA = 4,
#endif
#ifndef NO_SHA256
    WC_HASH_TYPE_SHA256 = 5,
#endif
#ifdef WOLFSSL_SHA512
#ifdef WOLFSSL_SHA384
    WC_HASH_TYPE_SHA384 = 6,
#endif /* WOLFSSL_SHA384 */
    WC_HASH_TYPE_SHA512 = 7,
#endif /* WOLFSSL_SHA512 */
};

WOLFSSL_API int wc_HashGetDigestSize(enum wc_HashType hash_type);
WOLFSSL_API int wc_Hash(enum wc_HashType hash_type,
    const byte* data, word32 data_len,
    byte* hash, word32 hash_len);


#ifndef NO_MD5
#include <wolfssl/wolfcrypt/md5.h>
WOLFSSL_API void wc_Md5GetHash(Md5*, byte*);
WOLFSSL_API void wc_Md5RestorePos(Md5*, Md5*);
#if defined(WOLFSSL_TI_HASH)
    WOLFSSL_API void wc_Md5Free(Md5*);
#else
    #define wc_Md5Free(d)
#endif
#endif

#ifndef NO_SHA
#include <wolfssl/wolfcrypt/sha.h>
WOLFSSL_API int wc_ShaGetHash(Sha*, byte*);
WOLFSSL_API void wc_ShaRestorePos(Sha*, Sha*);
WOLFSSL_API int wc_ShaHash(const byte*, word32, byte*);
#if defined(WOLFSSL_TI_HASH)
     WOLFSSL_API void wc_ShaFree(Sha*);
#else
    #define wc_ShaFree(d)
#endif
#endif

#ifndef NO_SHA256
#include <wolfssl/wolfcrypt/sha256.h>
WOLFSSL_API int wc_Sha256GetHash(Sha256*, byte*);
WOLFSSL_API void wc_Sha256RestorePos(Sha256*, Sha256*);
WOLFSSL_API int wc_Sha256Hash(const byte*, word32, byte*);
#if defined(WOLFSSL_TI_HASH)
    WOLFSSL_API void wc_Sha256Free(Sha256*);
#else
    #define wc_Sha256Free(d)
#endif
#endif

#ifdef WOLFSSL_SHA512
#include <wolfssl/wolfcrypt/sha512.h>
WOLFSSL_API int wc_Sha512Hash(const byte*, word32, byte*);
#if defined(WOLFSSL_TI_HASH)
    WOLFSSL_API void wc_Sha512Free(Sha512*);
#else
    #define wc_Sha512Free(d)
#endif
    #if defined(WOLFSSL_SHA384)
        WOLFSSL_API int wc_Sha384Hash(const byte*, word32, byte*);
        #if defined(WOLFSSL_TI_HASH)
            WOLFSSL_API void wc_Sha384Free(Sha384*);
        #else
            #define wc_Sha384Free(d)
        #endif
    #endif /* defined(WOLFSSL_SHA384) */
#endif /* WOLFSSL_SHA512 */


#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* WOLF_CRYPT_HASH_H */
