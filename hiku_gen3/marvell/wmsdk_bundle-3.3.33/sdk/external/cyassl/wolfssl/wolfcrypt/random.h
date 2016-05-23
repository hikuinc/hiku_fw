/* random.h
 *
 * Copyright (C) 2006-2015 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of wolfSSL.
 *
 * Contact licensing@wolfssl.com with any questions or comments.
 *
 * http://www.wolfssl.com
 */



#ifndef WOLF_CRYPT_RANDOM_H
#define WOLF_CRYPT_RANDOM_H

#include <wolfssl/wolfcrypt/types.h>

#ifdef HAVE_FIPS
/* for fips @wc_fips */
#include <cyassl/ctaocrypt/random.h>
#endif

#ifdef __cplusplus
    extern "C" {
#endif

#ifndef HAVE_FIPS /* avoid redefining structs and macros */
#if defined(WOLFSSL_FORCE_RC4_DRBG) && defined(NO_RC4)
    #error Cannot have WOLFSSL_FORCE_RC4_DRBG and NO_RC4 defined.
#endif /* WOLFSSL_FORCE_RC4_DRBG && NO_RC4 */
#if defined(HAVE_HASHDRBG) || defined(NO_RC4)
    #ifdef NO_SHA256
        #error "Hash DRBG requires SHA-256."
    #endif /* NO_SHA256 */

    #include <wolfssl/wolfcrypt/sha256.h>
#else /* HAVE_HASHDRBG || NO_RC4 */
    #include <wolfssl/wolfcrypt/arc4.h>
#endif /* HAVE_HASHDRBG || NO_RC4 */

#if defined(USE_WINDOWS_API)
    #if defined(_WIN64)
        typedef unsigned __int64 ProviderHandle;
        /* type HCRYPTPROV, avoid #include <windows.h> */
    #else
        typedef unsigned long ProviderHandle;
    #endif
#endif


/* OS specific seeder */
typedef struct OS_Seed {
    #if defined(USE_WINDOWS_API)
        ProviderHandle handle;
    #else
        int fd;
    #endif
} OS_Seed;

#if defined(HAVE_HASHDRBG) || defined(NO_RC4)


#define DRBG_SEED_LEN (440/8)


struct DRBG; /* Private DRBG state */


/* Hash-based Deterministic Random Bit Generator */
typedef struct WC_RNG {
    struct DRBG* drbg;
    OS_Seed seed;
    byte status;
} WC_RNG;


#else /* HAVE_HASHDRBG || NO_RC4 */


#define WOLFSSL_RNG_CAVIUM_MAGIC 0xBEEF0004

/* secure Random Number Generator */


typedef struct WC_RNG {
    OS_Seed seed;
    Arc4    cipher;
#ifdef HAVE_CAVIUM
    int    devId;           /* nitrox device id */
    word32 magic;           /* using cavium magic */
#endif
} WC_RNG;


#endif /* HAVE_HASH_DRBG || NO_RC4 */

#endif /* HAVE_FIPS */

/* NO_OLD_RNGNAME removes RNG struct name to prevent possible type conflicts,
 * can't be used with CTaoCrypt FIPS */
#if !defined(NO_OLD_RNGNAME) && !defined(HAVE_FIPS)
    #define RNG WC_RNG
#endif

WOLFSSL_LOCAL
int wc_GenerateSeed(OS_Seed* os, byte* seed, word32 sz);

#if defined(HAVE_HASHDRBG) || defined(NO_RC4)

#ifdef HAVE_CAVIUM
    WOLFSSL_API int  wc_InitRngCavium(WC_RNG*, int);
#endif

#endif /* HAVE_HASH_DRBG || NO_RC4 */


WOLFSSL_API int  wc_InitRng(WC_RNG*);
WOLFSSL_API int  wc_RNG_GenerateBlock(WC_RNG*, byte*, word32 sz);
WOLFSSL_API int  wc_RNG_GenerateByte(WC_RNG*, byte*);
WOLFSSL_API int  wc_FreeRng(WC_RNG*);


#if defined(HAVE_HASHDRBG) || defined(NO_RC4)
    WOLFSSL_API int wc_RNG_HealthTest(int reseed,
                                        const byte* entropyA, word32 entropyASz,
                                        const byte* entropyB, word32 entropyBSz,
                                        byte* output, word32 outputSz);
#endif /* HAVE_HASHDRBG || NO_RC4 */

#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* WOLF_CRYPT_RANDOM_H */

