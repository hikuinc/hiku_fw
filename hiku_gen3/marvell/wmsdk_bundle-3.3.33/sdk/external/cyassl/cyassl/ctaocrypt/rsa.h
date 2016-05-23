/* rsa.h
 *
 * Copyright (C) 2006-2015 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of wolfSSL.
 *
 * Contact licensing@wolfssl.com with any questions or comments.
 *
 * http://www.wolfssl.com
 */


#ifndef NO_RSA

#ifndef CTAO_CRYPT_RSA_H
#define CTAO_CRYPT_RSA_H

#include <wolfssl/wolfcrypt/rsa.h>
/* includes for their compatibility */
#include <cyassl/ctaocrypt/integer.h>
#include <cyassl/ctaocrypt/random.h>
#include <cyassl/ctaocrypt/settings.h>

#define InitRsaKey       wc_InitRsaKey
#define FreeRsaKey       wc_FreeRsaKey
#define RsaPublicEncrypt wc_RsaPublicEncrypt
#define RsaPrivateDecryptInline wc_RsaPrivateDecryptInline
#define RsaPrivateDecrypt       wc_RsaPrivateDecrypt
#define RsaSSL_Sign             wc_RsaSSL_Sign
#define RsaSSL_VerifyInline     wc_RsaSSL_VerifyInline
#define RsaSSL_Verify           wc_RsaSSL_Verify
#define RsaEncryptSize          wc_RsaEncryptSize
#define RsaFlattenPublicKey     wc_RsaFlattenPublicKey

#ifdef WOLFSSL_KEY_GEN
    #define MakeRsaKey  wc_MakeRsaKey
    #define RsaKeyToDer wc_RsaKeyToDer
#endif

#ifdef HAVE_CAVIUM
    #define RsaInitCavium wc_RsaInitCavium
    #define RsaFreeCavium wc_RsaFreeCavium
#endif

#endif /* CTAO_CRYPT_RSA_H */

#endif /* NO_RSA */
