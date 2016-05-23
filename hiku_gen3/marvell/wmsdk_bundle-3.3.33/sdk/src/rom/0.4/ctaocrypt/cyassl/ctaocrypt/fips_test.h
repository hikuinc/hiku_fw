/* fips_test.h
 *
 * Copyright (C) 2006-2014 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@wolfssl.com with any questions or comments.
 *
 * http://www.wolfssl.com
 */



#ifndef CTAO_CRYPT_FIPS_TEST_H
#define CTAO_CRYPT_FIPS_TEST_H

#include <cyassl/ctaocrypt/types.h>


#ifdef __cplusplus
    extern "C" {
#endif

/* Known Answer Test string inputs are hex */

CYASSL_LOCAL int DoKnownAnswerTests(void);


#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* CTAO_CRYPT_FIPS_TEST_H */

