#ifndef __USER_SETTINGS_H__
#define __USER_SETTINGS_H__

/*
 * Configuration: Default
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

#define DEBUG_WOLFSSL

#endif /* __USER_SETTINGS_H__ */
