/* coding.h
 *
 * Copyright (C) 2006-2014 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@wolfssl.com with any questions or comments.
 *
 * http://www.wolfssl.com
 */



#ifndef CTAO_CRYPT_CODING_H
#define CTAO_CRYPT_CODING_H

#include <cyassl/ctaocrypt/types.h>

#ifdef __cplusplus
    extern "C" {
#endif


/* decode needed by CyaSSL */
CYASSL_LOCAL int Base64_Decode(const byte* in, word32 inLen, byte* out,
                               word32* outLen);

#if defined(OPENSSL_EXTRA) || defined(SESSION_CERTS) || defined(CYASSL_KEY_GEN)  || defined(CYASSL_CERT_GEN) || defined(HAVE_WEBSERVER)
    /* encode isn't */
    CYASSL_API
    int Base64_Encode(const byte* in, word32 inLen, byte* out,
                                  word32* outLen);
    CYASSL_API
    int Base64_EncodeEsc(const byte* in, word32 inLen, byte* out,
                                  word32* outLen);
#endif

#if defined(OPENSSL_EXTRA) || defined(HAVE_WEBSERVER) || defined(HAVE_FIPS)
    CYASSL_API
    int Base16_Decode(const byte* in, word32 inLen, byte* out, word32* outLen);
#endif


#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* CTAO_CRYPT_CODING_H */

