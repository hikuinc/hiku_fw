/* random.h
 *
 * Copyright (C) 2006-2014 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@wolfssl.com with any questions or comments.
 *
 * http://www.wolfssl.com
 */



#ifndef CTAO_CRYPT_RANDOM_H
#define CTAO_CRYPT_RANDOM_H

#include <cyassl/ctaocrypt/types.h>

#if defined(HAVE_HASHDRBG) || defined(NO_RC4)
    #ifdef NO_SHA256
        #error "Hash DRBG requires SHA-256."
    #endif /* NO_SHA256 */

    #include <cyassl/ctaocrypt/sha256.h>
#else /* HAVE_HASHDRBG || NO_RC4 */
    #include <cyassl/ctaocrypt/arc4.h>
#endif /* HAVE_HASHDRBG || NO_RC4 */

#ifdef __cplusplus
    extern "C" {
#endif


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


CYASSL_LOCAL
int GenerateSeed(OS_Seed* os, byte* seed, word32 sz);

#if defined(CYASSL_MDK_ARM)
#undef RNG
#define RNG CyaSSL_RNG   /* for avoiding name conflict in "stm32f2xx.h" */
#endif


#if defined(HAVE_HASHDRBG) || defined(NO_RC4)


#define DRBG_SEED_LEN (440/8)


/* Hash-based Deterministic Random Bit Generator */
typedef struct RNG {
    OS_Seed seed;

    Sha256 sha;
    byte digest[SHA256_DIGEST_SIZE];
    byte V[DRBG_SEED_LEN];
    byte C[DRBG_SEED_LEN];
    word32 reseedCtr;
    byte status;
} RNG;


#else /* HAVE_HASHDRBG || NO_RC4 */


#define CYASSL_RNG_CAVIUM_MAGIC 0xBEEF0004

/* secure Random Number Generator */


typedef struct RNG {
    OS_Seed seed;
    Arc4    cipher;
#ifdef HAVE_CAVIUM
    int    devId;           /* nitrox device id */
    word32 magic;           /* using cavium magic */
#endif
} RNG;


#ifdef HAVE_CAVIUM
    CYASSL_API int  InitRngCavium(RNG*, int);
#endif


#endif /* HAVE_HASH_DRBG || NO_RC4 */


typedef int (*GenerateSeed_cb)(OS_Seed* os, byte* output, word32 sz);
CYASSL_API int SetSeedGenerator(GenerateSeed_cb  gseed_function);
CYASSL_API int  InitRng(RNG*);
CYASSL_API int  RNG_GenerateBlock(RNG*, byte*, word32 sz);
CYASSL_API int  RNG_GenerateByte(RNG*, byte*);


#if defined(HAVE_HASHDRBG) || defined(NO_RC4)
    CYASSL_API int FreeRng(RNG*);
    CYASSL_API int RNG_HealthTest(int reseed,
                                        const byte* entropyA, word32 entropyASz,
                                        const byte* entropyB, word32 entropyBSz,
                                        const byte* output, word32 outputSz);
#endif /* HAVE_HASHDRBG || NO_RC4 */


#ifdef HAVE_FIPS
    /* fips wrapper calls, user can call direct */
    CYASSL_API int InitRng_fips(RNG* rng);
    CYASSL_API int FreeRng_fips(RNG* rng);
    CYASSL_API int RNG_GenerateBlock_fips(RNG* rng, byte* buf, word32 bufSz);
    CYASSL_API int RNG_HealthTest_fips(int reseed,
                                        const byte* entropyA, word32 entropyASz,
                                        const byte* entropyB, word32 entropyBSz,
                                        const byte* output, word32 outputSz);
    #ifndef FIPS_NO_WRAPPERS
        /* if not impl or fips.c impl wrapper force fips calls if fips build */
        #define InitRng              InitRng_fips
        #define FreeRng              FreeRng_fips
        #define RNG_GenerateBlock    RNG_GenerateBlock_fips
        #define RNG_HealthTest       RNG_HealthTest_fips
    #endif /* FIPS_NO_WRAPPERS */
#endif /* HAVE_FIPS */


#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* CTAO_CRYPT_RANDOM_H */

