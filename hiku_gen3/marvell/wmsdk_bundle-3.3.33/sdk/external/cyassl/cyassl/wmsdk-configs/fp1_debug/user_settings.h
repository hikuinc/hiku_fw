#ifndef __USER_SETTINGS_H__
#define __USER_SETTINGS_H__

/*
 * Configuration: AES CCM
 */

#define ENABLE_MCU_AES_HW_ACCL

#define WOLFSSL_AES_COUNTER
#define NO_ERROR_STRINGS

#ifndef NO_FILESYSTEM
#define NO_FILESYSTEM
#endif


#define NO_DEV_RANDOM
#define NO_PSK

#define HAVE_SNI
#define HAVE_TLS_EXTENSIONS

#define HAVE_AESCCM
/* Below two are needed for AES CCM */
#define WOLFSSL_SHA384
#define WOLFSSL_SHA512

#define DEBUG_WOLFSSL

#endif /* __USER_SETTINGS_H__ */
