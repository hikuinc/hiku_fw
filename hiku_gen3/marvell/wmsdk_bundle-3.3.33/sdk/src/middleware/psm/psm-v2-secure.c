/*! \file psm-v2-secure.c
 *  \brief Persistent storage manager version 2
 *
 *  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 *
 */

#include <wm_os.h>
#include <psm-v2.h>
#include "psm-v2-secure.h"
#include <mdev_aes.h>
#include <keystore.h>
#include "psm-internal.h"

/*
 * To encrypt (and decrypt) for each PSM object the "name + value"
 * together is considered as a single unit. 'resetkey' function is called at
 * the start of enc/dec operation for every object.
 *
 * Note that IV/nonce will be repeated for each PSM object. As of now each
 * object encryption starts will same IV and key.
 */

typedef struct {
	const uint8_t *key;
	const uint8_t *iv;
	uint16_t key_sz;
	aes_t enc_aes;
	mdev_t *aes_dev;
} sec_data_t;

#ifdef CONFIG_CPU_MC200
#define HARDCODED_KEY
#endif /* CONFIG_CPU_MC200 */

#ifdef HARDCODED_KEY
const byte ctr_key[] = {
	0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
	0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};

const byte ctr_iv[] = {
	0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
	0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};
#endif /* HARDCODED_KEY */

static int psm_keystore_get_data(sec_data_t *sec_data)
{
#ifndef HARDCODED_KEY
	/* get encryption key & key_size from keystore */
	psm_d("Using secure keystore");
	int rv = keystore_get_ref(KEY_PSM_ENCRYPT_KEY, &sec_data->key_sz,
				  &sec_data->key);
	if (rv == ERR_TLV_NOT_FOUND) {
		psm_e("psm: Could not read key key_size from keystore"
		      "Please flash correct boot2");
		return -WM_E_NOENT;
	}

	/* get nonce from keystore */
	rv = keystore_get_ref(KEY_PSM_NONCE, NULL, &sec_data->iv);
	if (rv == ERR_TLV_NOT_FOUND) {
		psm_e("psm: Could not read nonce");
		return -WM_E_NOENT;
	}
#else
	sec_data->key = ctr_key;
	sec_data->key_sz = sizeof(ctr_key);
	sec_data->iv = ctr_iv;
#endif

	return WM_SUCCESS;
}

int psm_security_init(psm_sec_hnd_t *sec_handle)
{
	aes_drv_init();

	sec_data_t *sec_data = os_mem_calloc(sizeof(sec_data_t));
	if (!sec_data)
		return -WM_E_NOMEM;

	int rv = psm_keystore_get_data(sec_data);
	if (rv != WM_SUCCESS) {
		os_mem_free(sec_data);
		return rv;
	}

	sec_data->aes_dev = aes_drv_open(MDEV_AES_0);
	if (!sec_data->aes_dev) {
		wmprintf("psm_security_init FAILED\r\n");
		os_mem_free(sec_data);
		return -WM_FAIL;
	}

	*sec_handle = (psm_sec_hnd_t) sec_data;
	return WM_SUCCESS;
}

int psm_key_reset(psm_sec_hnd_t sec_handle, psm_resetkey_mode_t mode)
{
	int dir = -1;
	switch (mode) {
	case SET_ENCRYPTION_KEY:
		dir = AES_ENCRYPTION;
		break;
	case SET_DECRYPTION_KEY:
		dir = AES_DECRYPTION;
		break;
	}

	sec_data_t *sec_data = (sec_data_t *) sec_handle;
	return aes_drv_setkey(sec_data->aes_dev, &sec_data->enc_aes,
			      sec_data->key, sec_data->key_sz,
			      sec_data->iv, dir, HW_AES_MODE_CTR);

}

int psm_encrypt(psm_sec_hnd_t sec_handle, const void *plain, void *cipher,
		uint32_t len)
{
	sec_data_t *sec_data = (sec_data_t *) sec_handle;
	int rv = aes_drv_encrypt(sec_data->aes_dev, &sec_data->enc_aes,
				 plain, cipher, len);

	return rv;
}

int psm_decrypt(psm_sec_hnd_t sec_handle, const void *cipher, void *plain,
		uint32_t len)
{
	sec_data_t *sec_data = (sec_data_t *) sec_handle;
	return aes_drv_decrypt(sec_data->aes_dev, &sec_data->enc_aes,
			       cipher, plain, len);
}

void psm_security_deinit(psm_sec_hnd_t *sec_handle)
{
	sec_data_t *sec_data = (sec_data_t *) *sec_handle;
	aes_drv_close(sec_data->aes_dev);
	os_mem_free(sec_data);
}
