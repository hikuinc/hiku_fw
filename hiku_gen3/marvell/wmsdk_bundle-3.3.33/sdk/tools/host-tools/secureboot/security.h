/*
 * Copyright (C) 2015, Marvell International Ltd.
 * All Rights Reserved.
 */

#ifndef	_SECURITY_H_
#define _SECURITY_H_

#include <stdint.h>

/** Helper macros to determine the support for an algorithm */
#define MOD_BOOT2 0
#define MOD_FW 1

#define true 1
#define false 0

static inline int module_supports_algo(uint8_t *algo_list,
	uint8_t algo_list_size, uint8_t algo_type)
{
	int i;
	for (i = 0; i < algo_list_size; i++)
		if (algo_list[i] == algo_type)
			return true;

	return false;
}

extern uint8_t boot2_encrypt_algos[], boot2_signing_algos[], boot2_hash_algos[];
extern uint8_t boot2_encrypt_algos_size, boot2_signing_algos_size,
		boot2_hash_algos_size;

#define BOOT2_SUPPORTS_ENCRYPT_ALGO(algo_type) module_supports_algo( \
		boot2_encrypt_algos, boot2_encrypt_algos_size, algo_type)
#define BOOT2_SUPPORTS_SIGNING_ALGO(algo_type) module_supports_algo( \
		boot2_signing_algos, boot2_signing_algos_size, algo_type)
#define BOOT2_SUPPORTS_HASH_ALGO(algo_type) module_supports_algo( \
		boot2_hash_algos, boot2_hash_algos_size, algo_type)

extern uint8_t firmware_encrypt_algos[], firmware_signing_algos[],
		firmware_hash_algos[];
extern uint8_t firmware_encrypt_algos_size, firmware_signing_algos_size,
		firmware_hash_algos_size;

#define FIRMWARE_SUPPORTS_ENCRYPT_ALGO(algo_type) module_supports_algo( \
		firmware_encrypt_algos, firmware_encrypt_algos_size, algo_type)
#define FIRMWARE_SUPPORTS_SIGNING_ALGO(algo_type) module_supports_algo( \
		firmware_signing_algos, firmware_signing_algos_size, algo_type)
#define FIRMWARE_SUPPORTS_HASH_ALGO(algo_type) module_supports_algo( \
		firmware_hash_algos, firmware_hash_algos_size, algo_type)

typedef struct security_t security_t;

/** Type of encryption algorithm query
 */
typedef enum {
	ENC_IV_LEN = 0
} query_t;

/**  Prototype for the encrypt initialization function. Please use a
 * wrapper for your function if it does not match this signature.
 *
 * @pre initialize_encryption_context()
 *
 * @param[in,out] security A pointer to the security context.
 * @param[in] key Key to be used for encryption.
 * @param[in] key_len Length of encryption key.
 * @param[in] iv The initialization vector.
 * @param[in] iv_len Length of the initialization vector.
 *
 * @return 0 on success
 * @return Negative error code on failure.
 */
typedef int (*encrypt_init_func_t) (security_t *security, const void *key,
			int key_len, const void *iv, int iv_len);

/**  Prototype for the encrypt query function. Please use a
 * wrapper for your function if it does not match this signature.
 *
 * @pre initialize_encryption_context()
 *
 * @param[in] query Type of query of type \ref query_t
 *
 * @return query value on success
 * @return Negative error code on failure.
 */
typedef int (*encrypt_query_func_t) (query_t query);

/** Prototype for the encrypt function. Please use a wrapper for your
 * function if it does not match this signature.
 *
 * @note At the moment, multiply calling this function is not allowed.
 * encrypt_init_func() must be called for every call of this function.
 *
 * @pre encrypt_init_func
 *
 * @param[in,out] security A pointer to the security context.
 * @param[in] Pointer to the input buffer which has the input text.
 * @param[in] inlen Length of the input buffer (number of bytes).
 * @param[out] outbuf A pointer to the output buffer which will hold
 * the encrypted data. It can be same as the inbuf if the algorithm
 * uses in-place decryption. Memory for outbuf will be allocated by this
 * function and it is the responsibility of user to free it.
 * @param[out] mic A pointer to a buffer which will hold the message integrity
 * code (mic). If the algorithm does not produce a mic, this will be NULL.
 * Memory for mic buffer will be allocated by this function and it is the
 * responsibility of user to free it
 * @param[out] mic_len Length of the mic buffer, (0 if no mic is generated.)
 *
 * @return len Length of the output buffer.
 * @return Negative error code on failure.
 */
typedef int (*encrypt_func_t) (security_t *security, void *inbuf, int inlen,
			void **outbuf, void **mic, int *miclen);

/** Prototype for the hash initialization function. Please use a
 * wrapper for your function if it does not match this signature.
 *
 * @pre initialize_hash_context()
 *
 * @param[in,out] security A pointer to the security context.
 *
 * @return 0 on success
 * @return Negative error code on failure.
 */
typedef int (*hash_init_func_t) (security_t *security);

/** Prototype for the hash update function. Please use a wrapper for your
 * function if it does not match this signature.
 *
 * @note This function can be multiply called to update the cumulative
 * hash value calculated so far.
 *
 * @pre hash_init_func
 *
 * @param[in,out] security A pointer to the security context.
 * @param[in] buf Pointer to a buffer whose hash (checksum) is to be
 * calculated.
 * @param[in] buflen Length of the buffer
 */
typedef int (*hash_update_func_t) (security_t *security, const void *buf,
			int buflen);

/** Prototype for the hash finish function. Please use a wrapper for your
 * function if it does not match this signature.
 *
 * This function finalizes the hash result of input text passed through
 * one or more calls to hash_update_func(). After this, hash_init_func()
 * must be called again to start calculating new hash result.
 *
 * @pre hash_update_func
 *
 * @param[in,out] security A pointer to the security context.
 * @param[out] result Pointer to a buffer where the result of the hash
 * (checksum) will be stored.
 *
 * @return len Length of the hash result buffer.
 * @return Negative error code on failure.
 */
typedef int (*hash_finish_func_t) (security_t *security, void **result);

/** Prototype for the message signing initialization function. Please use a
 * wrapper for your function if it does not match this signature.
 *
 * @pre initialize_signing_context()
 *
 * @param[in,out] security A pointer to the security context.
 * @param[in] private_key Private key to be used to sign the input text
 * @param[in] key_len Length of the private key
 *
 * @return 0 on success
 * @return Negative error code on failure.
 */
typedef int (*sign_init_func_t) (security_t *security, const void *private_key,
			int key_len);

/** Prototype for the message signing function. Please use a wrapper
 * for your function if it does not match this signature.
 *
 * @note This function can be multiply called between sign_func_init()
 * and sign_func_deinit() to generate independent signatures.
 *
 * @pre sign_init_func
 *
 * @param[in,out] security A pointer to the security context.
 * @param[in] msg Pointer to a buffer which holds the message whose
 * signature has to be calculated.
 * @param[in] msglen Length of the message
 * @param[out] sign The buffer that will hold the calculated signature.
 * Memory for the buffer will be allocated by this function and it is the
 * responsibility of user to free this memory.
 *
 * @return len Length of the sign buffer.
 * @return Negative error code on failure.
 */
typedef int (*sign_func_t) (security_t *security, const void *message, int len,
			void **sign);

/** Prototype for the message signing deinitialization function. Please use a
 * wrapper for your function if it does not match this signature.
 *
 * @pre sign_func_init
 * @param[in,out] security A pointer to the security context.
 */
typedef void (*sign_deinit_func_t) (security_t *security);

/** Algorithm agnostic security  structure
 */
struct security_t {
	/** Pointer to the encryption initialization function
	 * of type \ref encrypt_init_func_t
	 */
	encrypt_init_func_t encrypt_init;
	/** Pointer to the encryption query function of type
	 * \ref encrypt_query_func_t
	 */
	encrypt_query_func_t encrypt_query;
	/** Pointer to the encryption function of type
	 * \ref encrypt_func_t
	 */
	encrypt_func_t encrypt;
	/** Pointer to the hash initialization function
	 * of type \ref hash_init_func_t
	 */
	hash_init_func_t hash_init;
	/** Pointer to the hash update function of type
	 * \ref hash_update_func_t
	 */
	hash_update_func_t hash_update;
	/** Pointer to a hash finish function of type
	 * \ref hash_finish_func_t
	 */
	hash_finish_func_t hash_finish;
	/** Pointer to the message signing initialization function
	 * of type \ref sign_init_func_t
	 */
	sign_init_func_t sign_init;
	/** Pointer to the message signing function of type
	 * \ref sign_func_t
	 */
	sign_func_t sign;
	/** Pointer to the message signing deinitialization function
	 * of type \ref sign_deinit_func_t
	 */
	sign_deinit_func_t sign_deinit;
	/** Pointer to the encrypt context initialized by the encrypt_init
	 */
	void *encrypt_ctx;
	/** Initialization vector (IV)  as per the encryption algorithm.
	 * To be used internally by encrypt_init and encrypt function if needed.
	 */
	const void *iv;
	/** Length of the IV as per the encryption algorithm being used.
	 * To be used internally by encrypt_init and encrypt function if needed.
	 */
	int iv_len;
	/** Pointer to the hash context initialized by the hash_init
	 */
	void *hash_ctx;
	/** Pointer to the message signing context initialized by sign_init.
	 */
	void *sign_ctx;
};

/* This function initializes the security context with appropriate algorithm
 * specific parameters and callbacks.
 *
 * @note This function must be updated when new encryption algorithm is added.
 *
 * @param[in,out] security A pointer to the security context.
 * @param[in] encrypt_type Encryption algorithm type to use.
 *
 * @return encrypt_type if algorithm is supported.
 * @return NO_ENCRYPT if algorithm is unsupported.
 */
int initialize_encryption_context(security_t *security, uint8_t encrypt_type);

/* This function initializes the security context with appropriate algorithm
 * specific parameters and callbacks.
 *
 * @note This function must be updated when new hash algorithm is added.
 *
 * @param[in,out] security A pointer to the security context.
 * @param[in] hash_type Hash algorithm type to use.
 *
 * @return hash_type if algorithm is supported.
 * @return NO_HASH if algorithm is unsupported.
 */
int initialize_hash_context(security_t *security, uint8_t hash_type);

/* This function initializes the security context with appropriate algorithm
 * specific parameters and callbacks.
 *
 * @note This function must be updated when new signing algorithm is added.
 *
 * @param[in,out] security A pointer to the security context.
 * @param[in] signing_type Message signing algorithm type to use.
 *
 * @return signing_type if algorithm is supported.
 * @return NO_SIGN if algorithm is unsupported.
 */
int initialize_signing_context(security_t *security, uint8_t signing_type);
#endif /*_SECURITY_H_ */

