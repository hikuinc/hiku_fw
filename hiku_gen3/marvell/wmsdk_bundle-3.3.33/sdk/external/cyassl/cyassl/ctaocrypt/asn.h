/* asn.h
 *
 * Copyright (C) 2006-2015 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of wolfSSL.
 *
 * Contact licensing@wolfssl.com with any questions or comments.
 *
 * http://www.wolfssl.com
 */


#ifndef NO_ASN

#ifndef CTAO_CRYPT_ASN_H
#define CTAO_CRYPT_ASN_H

/* pull in compatibility for each include */
#include <cyassl/ctaocrypt/dh.h>
#include <cyassl/ctaocrypt/dsa.h>
#include <cyassl/ctaocrypt/sha.h>
#include <cyassl/ctaocrypt/md5.h>
#include <cyassl/ctaocrypt/asn_public.h>   /* public interface */
#ifdef HAVE_ECC
    #include <cyassl/ctaocrypt/ecc.h>
#endif


#include <wolfssl/wolfcrypt/asn.h>

#ifndef WOLFSSL_PEMCERT_TODER_DEFINED
#ifndef NO_FILESYSTEM
    #define CyaSSL_PemCertToDer wolfSSL_PemCertToDer
#endif
#endif

#endif /* CTAO_CRYPT_ASN_H */

#endif /* !NO_ASN */

