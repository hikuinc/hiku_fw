#ifndef __USER_SETTINGS_H__
#define __USER_SETTINGS_H__

/*
 * Configuration: For SEP 2.0
 */

/* AES CCM is not working with Hardware Accelarator. */
/* #define ENABLE_MCU_AES_HW_ACCL */

#define WOLFSSL_AES_COUNTER
#define NO_ERROR_STRINGS

#ifndef NO_FILESYSTEM
#define NO_FILESYSTEM
#endif

#define NO_DEV_RANDOM
#define NO_PSK
#define HAVE_ECC
#define NO_RSA
#define NO_DH
#define NO_DES
#define NO_DES3
#define NO_RC4
#define NO_SHA_X
#define HAVE_AESCCM

#define HAVE_SNI
#define HAVE_TLS_EXTENSIONS

#endif /* __USER_SETTINGS_H__ */
