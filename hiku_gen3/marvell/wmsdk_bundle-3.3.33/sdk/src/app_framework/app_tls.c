/*
 *  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* app_tls.c: This file has some functions defined to simplify working
   with the tls library
 */

#include <string.h>
#include <stdlib.h>

#include <wmassert.h>
#include <wmstdio.h>
#include <wm_utils.h>
#include <app_framework.h>
#include <wm-tls.h>
#include <ftfs.h>

#include "app_dbg.h"

/*
 * The buffer cert is allocated on heap and caller is supposed to free
 * it. The return value is size of the loaded file.
 */
static int _load_tls_cert_to_buffer(struct fs *fs,
				    const char *filename, void **buffer)
{
	app_d("%s: %s", __func__, filename);
	int cert_sz = ftfs_get_filesize(fs, filename);
	if (cert_sz <= 0) {
		return -WM_FAIL;
	}

	app_d("%s cert size is %d", __func__, cert_sz);
	void *cert_buf = os_mem_alloc(cert_sz);
	if (!cert_sz)
		return -WM_E_NOMEM;

	int rv = ftfs_load_entire_file_to_buffer(fs, filename,
						 cert_buf, cert_sz);
	if (rv <= 0) {
		os_mem_free(cert_buf);
		return -WM_FAIL;
	}

	*buffer = cert_buf;
	return rv;
}

SSL_CTX *app_tls_create_client_context_ftfs(struct fs *fs,
					    const app_tls_client_ftfs_t *cfg)
{
	if (!cfg || !fs)
		return NULL;

	tls_client_t lcfg;
	memset(&lcfg, 0x00, sizeof(tls_client_t));

	SSL_CTX *ctx = NULL;
	int rv;
	void *tmp;
	if (cfg->ca_cert_filename) {
		app_d("Loading CA cert");
		rv = _load_tls_cert_to_buffer(fs, cfg->ca_cert_filename, &tmp);
		if (rv < 0)
			goto exit_tls_ccc_error;

		lcfg.ca_cert = tmp;
		lcfg.ca_cert_size = rv;
	}

	if (cfg->client_cert_filename) {
		app_d("Loading client cert");
		rv = _load_tls_cert_to_buffer(fs, cfg->client_cert_filename,
					      &tmp);
		if (rv < 0)
			goto exit_tls_ccc_error;

		lcfg.client_cert = tmp;
		lcfg.client_cert_size = rv;
		lcfg.client_cert_is_chained = cfg->client_cert_is_chained;

		if (!cfg->client_key_filename)
			goto exit_tls_ccc_error;

		app_d("Loading client key");
		rv = _load_tls_cert_to_buffer(fs, cfg->client_key_filename,
					      &tmp);
		if (rv < 0)
			goto exit_tls_ccc_error;

		lcfg.client_key = tmp;
		lcfg.client_key_size = rv;
	}

	ctx = tls_create_client_context(&lcfg);
exit_tls_ccc_error:
	if (lcfg.ca_cert)
		os_mem_free((void *)lcfg.ca_cert);
	if (lcfg.client_cert)
		os_mem_free((void *)lcfg.client_cert);
	if (lcfg.client_key)
		os_mem_free((void *)lcfg.client_key);

	return ctx;
}
