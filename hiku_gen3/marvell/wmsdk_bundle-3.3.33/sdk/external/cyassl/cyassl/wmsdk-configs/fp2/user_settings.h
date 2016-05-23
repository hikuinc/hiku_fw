#ifndef __USER_SETTINGS_H__
#define __USER_SETTINGS_H__

/*
 * Configuration: Low memory, No AES CCM
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

#define CYASSL_LOW_MEMORY

#endif /* __USER_SETTINGS_H__ */
