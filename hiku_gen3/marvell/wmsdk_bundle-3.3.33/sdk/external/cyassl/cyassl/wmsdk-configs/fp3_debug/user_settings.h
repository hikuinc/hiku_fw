#ifndef __USER_SETTINGS_H__
#define __USER_SETTINGS_H__

/*
 * Configuration: Low memory, No AES CCM, No DSA, No DES, No DES3, NO_RC4,
 * NO RABBIT, NO_HC128, NO_MD4, NO_MD2, NO_SHA512
 */

#define ENABLE_MCU_AES_HW_ACCL

#define WOLFSSL_AES_COUNTER
#define NO_ERROR_STRINGS

#ifndef NO_FILESYSTEM
#define NO_FILESYSTEM
#endif


#define NO_DEV_RANDOM
#define NO_PSK
#define NO_DSA
#define NO_DES
#define NO_DES3
#define NO_RC4
#define NO_RABBIT
#define NO_HC128
#define NO_MD4
#define NO_MD2
#define NO_SHA512

#define HAVE_SNI
#define HAVE_TLS_EXTENSIONS

#define CYASSL_LOW_MEMORY
#define NO_SESSION_CACHE
#define NO_CLIENT_CACHE
#define NO_OLD_TLS

#define DEBUG_WOLFSSL

#endif /* __USER_SETTINGS_H__ */
