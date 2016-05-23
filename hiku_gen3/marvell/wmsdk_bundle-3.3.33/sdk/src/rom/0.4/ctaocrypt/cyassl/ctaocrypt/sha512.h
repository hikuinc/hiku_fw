/* sha512.h
 *
 * Copyright (C) 2006-2014 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@wolfssl.com with any questions or comments.
 *
 * http://www.wolfssl.com
 */



#ifdef CYASSL_SHA512

#ifndef CTAO_CRYPT_SHA512_H
#define CTAO_CRYPT_SHA512_H

#include <cyassl/ctaocrypt/types.h>

#ifdef __cplusplus
    extern "C" {
#endif


/* in bytes */
enum {
    SHA512              =   4,   /* hash type unique */
    SHA512_BLOCK_SIZE   = 128,
    SHA512_DIGEST_SIZE  =  64,
    SHA512_PAD_SIZE     = 112 
};


/* Sha512 digest */
typedef struct Sha512 {
    word32  buffLen;   /* in bytes          */
    word32  loLen;     /* length in bytes   */
    word32  hiLen;     /* length in bytes   */
    word64  digest[SHA512_DIGEST_SIZE / sizeof(word64)];
    word64  buffer[SHA512_BLOCK_SIZE  / sizeof(word64)];
} Sha512;


CYASSL_API int InitSha512(Sha512*);
CYASSL_API int Sha512Update(Sha512*, const byte*, word32);
CYASSL_API int Sha512Final(Sha512*, byte*);
CYASSL_API int Sha512Hash(const byte*, word32, byte*);


#if defined(CYASSL_SHA384) || defined(HAVE_AESGCM)

/* in bytes */
enum {
    SHA384              =   5,   /* hash type unique */
    SHA384_BLOCK_SIZE   = 128,
    SHA384_DIGEST_SIZE  =  48,
    SHA384_PAD_SIZE     = 112 
};


/* Sha384 digest */
typedef struct Sha384 {
    word32  buffLen;   /* in bytes          */
    word32  loLen;     /* length in bytes   */
    word32  hiLen;     /* length in bytes   */
    word64  digest[SHA512_DIGEST_SIZE / sizeof(word64)]; /* for transform 512 */
    word64  buffer[SHA384_BLOCK_SIZE  / sizeof(word64)];
} Sha384;


CYASSL_API int InitSha384(Sha384*);
CYASSL_API int Sha384Update(Sha384*, const byte*, word32);
CYASSL_API int Sha384Final(Sha384*, byte*);
CYASSL_API int Sha384Hash(const byte*, word32, byte*);


#ifdef HAVE_FIPS
    /* fips wrapper calls, user can call direct */
    CYASSL_API int InitSha512_fips(Sha512*);
    CYASSL_API int Sha512Update_fips(Sha512*, const byte*, word32);
    CYASSL_API int Sha512Final_fips(Sha512*, byte*);
    #ifndef FIPS_NO_WRAPPERS
        /* if not impl or fips.c impl wrapper force fips calls if fips build */
        #define InitSha512   InitSha512_fips
        #define Sha512Update Sha512Update_fips
        #define Sha512Final  Sha512Final_fips
    #endif /* FIPS_NO_WRAPPERS */

    /* fips wrapper calls, user can call direct */
    CYASSL_API int InitSha384_fips(Sha384*);
    CYASSL_API int Sha384Update_fips(Sha384*, const byte*, word32);
    CYASSL_API int Sha384Final_fips(Sha384*, byte*);
    #ifndef FIPS_NO_WRAPPERS
        /* if not impl or fips.c impl wrapper force fips calls if fips build */
        #define InitSha384   InitSha384_fips
        #define Sha384Update Sha384Update_fips
        #define Sha384Final  Sha384Final_fips
    #endif /* FIPS_NO_WRAPPERS */

#endif /* HAVE_FIPS */


#endif /* CYASSL_SHA384 */

#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* CTAO_CRYPT_SHA512_H */
#endif /* CYASSL_SHA512 */
