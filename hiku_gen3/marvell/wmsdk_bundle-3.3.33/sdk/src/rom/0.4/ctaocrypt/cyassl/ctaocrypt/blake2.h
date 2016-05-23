/* blake2.h
 *
 * Copyright (C) 2006-2014 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@wolfssl.com with any questions or comments.
 *
 * http://www.wolfssl.com
 */



#ifdef HAVE_BLAKE2

#ifndef CTAOCRYPT_BLAKE2_H
#define CTAOCRYPT_BLAKE2_H

#include <cyassl/ctaocrypt/blake2-int.h>

#ifdef __cplusplus
    extern "C" {
#endif

/* in bytes, variable digest size up to 512 bits (64 bytes) */
enum {
    BLAKE2B_ID  = 7,   /* hash type unique */
    BLAKE2B_256 = 32   /* 256 bit type, SSL default */
};


/* BLAKE2b digest */
typedef struct Blake2b {
    blake2b_state S[1];         /* our state */
    word32        digestSz;     /* digest size used on init */
} Blake2b;


CYASSL_API int InitBlake2b(Blake2b*, word32);
CYASSL_API int Blake2bUpdate(Blake2b*, const byte*, word32);
CYASSL_API int Blake2bFinal(Blake2b*, byte*, word32);



#ifdef __cplusplus
    } 
#endif

#endif  /* CTAOCRYPT_BLAKE2_H */
#endif  /* HAVE_BLAKE2 */

