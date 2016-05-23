/*! \file psm-v2-secure.h
 *  \brief Persistent storage manager version 2 (secure)
 *
 *  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 *
 * This file is used in case of Secure PSM. The idea is to have easily
 * replacable security function. The security functions declared in this
 * file need to be implemented. There can be mupliple versions. One of them
 * can be selected at compile time.
 */
#include <psm-v2.h>

typedef void *psm_sec_hnd_t;

/** Enum values which will be passed back in callback function 'resetkey' */
typedef enum {
	SET_ENCRYPTION_KEY = 0,
	SET_DECRYPTION_KEY = 1,
} psm_resetkey_mode_t;

/**
 * Security Init.
 *
 * This function should  perform the initialization required for psm
 * encryption and decryption operations. This should do everything that is
 * required to enable encryption and decryption operations later. This
 * function is invoked when psm_module_init() is invoked.
 *
 * @param[out] sec_handle Pointer to handle. Will be populated by
 * psm_security_init()
 *
 * @return User defined value typically WM_SUCCESS or -WM_FAIL. Any
 * failure here will abort the ongoing PSM operation.
 */
int psm_security_init(psm_sec_hnd_t *sec_handle);

/**
 * Reset PSM key.
 *
 * This function should reset the encryption/decryption key and/or
 * nonce. This function will be called at the start of operation on every
 * PSM object.
 *
 * Every PSM object is regarded as a independant encryption/decryption unit
 * by the PSM. Hence, this function will be called at the start of
 * processing of any new object.
 *
 * @param[in] sec_handle The same handle returned by psm_security_init().
 * @param[in] mode The exact operation that will happen later for this
 * key.
 *
 * @return User defined value typically WM_SUCCESS or -WM_FAIL. Any
 * failure here will abort the ongoing PSM operation.
 */
int psm_key_reset(psm_sec_hnd_t sec_handle, psm_resetkey_mode_t mode);

/**
 * Encrypt PSM operation.
 *
 * This function should perform the encryption of psm data passed as a
 * parameter.
 *
 * @param[in] sec_handle The same handle returned by psm_security_init().
 * @param[in] plain Pointer to data to be encrypted (plain text)
 * @param[out] cipher Pointer to encrypted data (cipher text).
 * @param[in] len Length of data to be encrypted.
 *
 * @return User defined value typically WM_SUCCESS or -WM_FAIL. Any
 * failure here will abort the ongoing PSM operation.
 */
int psm_encrypt(psm_sec_hnd_t sec_handle, const void *plain, void *cipher,
		uint32_t len);

/**
 * Decrypt PSM operation.
 *
 * This function should perform the decryption of psm data passed as a
 * parameter.
 *
 * @param[in] sec_handle The same handle returned by psm_security_init().
 * @param[in] cipher Pointer to data to be decrypted (cipher text)
 * @param[out] plain Pointer to buffer where decrypted data (plain
 * text) should be stored. Note that cipher and plain point to
 * same buffer i.e. PSM-v2 expects in place decryption.
 * @param[in] len Length of data to be decrypted.
 *
 * @return User defined value typically WM_SUCCESS or -WM_FAIL. Any
 * failure here will abort the ongoing PSM operation.
 */
int psm_decrypt(psm_sec_hnd_t sec_handle, const void *cipher, void *plain,
		uint32_t len);

/**
 * Security de-init
 *
 * This function should perform the de-initialization required to
 * free up the resources used for psm encryption and decryption
 * operations. After this API no other security related function will be
 * invoked. The implementation can free up any resources it is using. This
 * function is invoked when psm_module_deinit() is invoked.
 *
 * @param[in] sec_handle Pointer to the same handle returned by
 * psm_security_init().
 */
void psm_security_deinit(psm_sec_hnd_t *sec_handle);
