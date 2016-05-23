/* arc4.h
 *
 * Copyright (C) 2006-2015 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of wolfSSL.
 *
 * Contact licensing@wolfssl.com with any questions or comments.
 *
 * http://www.wolfssl.com
 */


#ifndef CTAO_CRYPT_ARC4_H
#define CTAO_CRYPT_ARC4_H

/* for arc4 reverse compatibility */
#ifndef NO_RC4
#include <wolfssl/wolfcrypt/arc4.h>
    #define CYASSL_ARC4_CAVIUM_MAGIC WOLFSSL_ARC4_CAVIUM_MAGIC
    #define Arc4Process wc_Arc4Process
    #define Arc4SetKey wc_Arc4SetKey
    #define Arc4InitCavium wc_Arc4InitCavium
    #define Arc4FreeCavium wc_Arc4FreeCavium
#endif

#endif /* CTAO_CRYPT_ARC4_H */

