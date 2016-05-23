/*
 * Copyright (C) 2008-2015, Marvell International Ltd.
 * All Rights Reserved.
 */

#ifndef __SECURE_CONFIG_H__
#define __SECURE_CONFIG_H__

/* Enable JTAG after boot, UART/USB without password
 * Set this to 0 for disabling JTAG.
 */
#define JTAG_STATUS 1

/* Enable UART/USB boot
 * Set this to 0 for disabling UART/USB boot modes.
 */
#define UART_USB_BOOT 1

/* Enable encrypted image boot
 * Set this to 0 for disabling encrypted boot mode.
 */
#define ENCRYPTED_BOOT 1

/* Enable signed image boot
 * Set this to 1 for enabling signed boot mode.
 */
#define SIGNED_BOOT 0

/* AES-CCM 256 bit key, 64 hex characters */
#define AES_CCM256_KEY \
	"004ebd4204fc8aa3b27c732e3f01bbcf71e93f1b6cd0153cf94e5bb2112e1041"

/* SHA256 checksum of RSA Public Key, 64 hex characters */
#define RSA_PUB_KEY_SHA256 \
	"0c8f52f08099b7207ad47d87d324decf54e677a85418084d39fe61e496b54dd1"

/* Enable writing user specific data to OTP memory
 * Set this to 1 for enabling user data write.
 * Note: Maximum size for user data can be up-to 252 bytes.
 */
#define USER_DATA 0

/* User specific additional data in OTP memory */
#define OTP_USER_DATA \
	"aa55aa55aa55aa55aa55aa55aa55aa55"

#endif /* __SECURE_CONFIG_H__ */
