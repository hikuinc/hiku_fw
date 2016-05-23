/* compress.h
 *
 * Copyright (C) 2006-2014 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@wolfssl.com with any questions or comments.
 *
 * http://www.wolfssl.com
 */



#ifdef HAVE_LIBZ

#ifndef CTAO_CRYPT_COMPRESS_H
#define CTAO_CRYPT_COMPRESS_H


#include <cyassl/ctaocrypt/types.h>


#ifdef __cplusplus
    extern "C" {
#endif


#define COMPRESS_FIXED 1


CYASSL_API int Compress(byte*, word32, const byte*, word32, word32);
CYASSL_API int DeCompress(byte*, word32, const byte*, word32);


#ifdef __cplusplus
    } /* extern "C" */
#endif


#endif /* CTAO_CRYPT_COMPRESS_H */

#endif /* HAVE_LIBZ */

