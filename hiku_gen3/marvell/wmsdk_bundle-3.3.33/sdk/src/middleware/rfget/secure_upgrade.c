/*
 *  Copyright 2014, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <stdlib.h>
#include <secure_upgrade.h>
#include <partition.h>
#include <rfget.h>
#include <httpc.h>

static uint32_t fw_upgrade_spec_version = 2;
static uint32_t get_u32_le(const void *vp)
{
	const uint8_t *p = (const uint8_t *)vp;
	uint32_t v;

	v  = (uint32_t)p[0];
	v |= (uint32_t)p[1] << 8;
	v |= (uint32_t)p[2] << 16;
	v |= (uint32_t)p[3] << 24;

	return v;
}
static int __httpc_fw_data_read_fn(void *priv, void *buf, uint32_t max_len)
{
	http_session_t *handle = (http_session_t *)priv;
	return http_read_content(*handle, buf, max_len);
}

static int upgrade_data_validate_cb(void *priv_data)
{
	upgrade_struct_t *priv = (upgrade_struct_t *)priv_data;
	if (!priv)
		return -WM_FAIL;
	priv->hash_finish(priv->hash_ctx, priv->hash);
	if (priv->signature_verify(priv->hash, priv->hash_len,
				priv->pk, priv->signature) != 0)
		return -WM_FAIL;
	else
		return WM_SUCCESS;
}
static size_t upgrade_data_fetch_cb(void *priv_data, void *buf,
		size_t max_len)
{
	int len;
	upgrade_struct_t *priv = (upgrade_struct_t *)priv_data;
	if (!priv)
		return -WM_FAIL;

	if (!priv->fw_data_read_fn)
		return -WM_FAIL;
	if (priv->bytes_to_read <= 0)
		return -WM_FAIL;
	/* Read the data and decrypt it */

	len = priv->fw_data_read_fn(priv->session_priv_data, buf, max_len);
	if (len < 0)
		return len;

	len = priv->decrypt(priv->decrypt_ctx, buf, len, buf, len);

	/* Update the hash with the decrypted data */
	priv->hash_update(priv->hash_ctx, buf, len);
	priv->bytes_to_read -= len;
	if (priv->bytes_to_read < 0)
		return -WM_FAIL;

	return len;
}

/* This wrapper ensures that the buffer is filled with max_len bytes */
static int http_read_content_wrapper(upgrade_struct_t *priv, void *buf,
		uint32_t max_len)
{
	uint32_t bytes_read = 0, cnt;
	while (bytes_read < max_len) {
		if (!priv->fw_data_read_fn)
			return -WM_FAIL;
		cnt = priv->fw_data_read_fn(priv->session_priv_data,
				buf + bytes_read,
				max_len - bytes_read);
		if (cnt <= 0)
			return -WM_FAIL;
		bytes_read += cnt;
	}
	return bytes_read;
}

int secure_upgrade(enum flash_comp flash_comp_type, upgrade_struct_t *priv,
		struct partition_entry *p)
{
	uint8_t tmp_buf[4];
	int status = -WM_FAIL;
		if (!priv->fw_data_read_fn)
			return -WM_FAIL;
	rfget_init();

	/* Read the Firmware spec version and compare with the expected
	 * version. If it is less than the current version, abort the
	 * process.
	 */
	if (http_read_content_wrapper(priv, tmp_buf,
				sizeof(tmp_buf)) <= 0)
		return -WM_FAIL;

	uint32_t src_fw_upgrade_spec_version = get_u32_le(tmp_buf);

	/* A check for 0xffffff has also been added because for version 1
	 * of the FW upgrade spec, the field was in Big Endian format.
	 * The current version uses Little Endian format. Hence, an image
	 * with version 1 will reflect as 0x01000000.
	 */
	if ((src_fw_upgrade_spec_version < fw_upgrade_spec_version) ||
			(src_fw_upgrade_spec_version > 0xffffff)) {
		rf_e("Incorrect Firmware Upgrade Specification version");
		return -WM_FAIL;
	}

	/* Read the Header length */
	if (http_read_content_wrapper(priv, tmp_buf,
				sizeof(tmp_buf)) <= 0)
		return -WM_FAIL;

	int32_t header_len = get_u32_le(tmp_buf);

	/* Read the Firmware Length */
	if (http_read_content_wrapper(priv, tmp_buf,
				sizeof(tmp_buf)) <= 0)
		return -WM_FAIL;

	uint32_t fw_len = get_u32_le(tmp_buf);

	/* Read the IV and initialize the decrypt context with it */
	if (http_read_content_wrapper(priv, priv->iv,
				priv->iv_len) <= 0)
		return -WM_FAIL;
	priv->decrypt_init(priv->decrypt_ctx, priv->decrypt_key,
			priv->iv);

	/* Read any additional bytes of header and discard them */
	header_len -= (priv->iv_len + sizeof(fw_len));
	if (header_len < 0) {
		return -WM_FAIL;
	}
	if (header_len > 0) {
		char dummy;
		while (header_len) {
			if (http_read_content_wrapper(priv, &dummy, 1)
					<= 0)
				return -WM_FAIL;
			header_len--;
		}
	}
	/* The data here after is encrypted data */

	/* Read the signature and decrypt it */
	if (http_read_content_wrapper(priv, priv->signature,
			priv->signature_len) <= 0)
		return -WM_FAIL;
	priv->decrypt(priv->decrypt_ctx, priv->signature, priv->signature_len,
			priv->signature, priv->signature_len);

	/* bytes_to_read is the size of the firmware */
	priv->bytes_to_read = fw_len;

	/* Start the Fimrware Upgrade Process */
	if (flash_comp_type == FC_COMP_FW)
		status = update_and_validate_firmware(
				upgrade_data_fetch_cb,
				(void *)priv, priv->bytes_to_read,
				p, upgrade_data_validate_cb);
#ifdef CONFIG_WIFI_FW_UPGRADE
	else if (flash_comp_type == FC_COMP_WLAN_FW)
		status = update_and_validate_wifi_firmware(
				upgrade_data_fetch_cb,
				(void *)priv, priv->bytes_to_read,
				p, upgrade_data_validate_cb);
#endif

	return status;
}

static struct partition_entry *__get_passive_partition(
		 enum flash_comp flash_comp_type)
{
	if (flash_comp_type == FC_COMP_FW)
		return rfget_get_passive_firmware();
	else if (flash_comp_type == FC_COMP_WLAN_FW)
		return rfget_get_passive_wifi_firmware();
	else
		return NULL;
}

int client_mode_secure_upgrade(enum flash_comp flash_comp_type, char *fw_url,
		upgrade_struct_t *priv, const tls_client_t *cfg)
{
	int status;
	struct partition_entry *p = __get_passive_partition(flash_comp_type);
	if (p == NULL) {
		rf_e("Failed to get passive partition");
		return -WM_FAIL;
	} else
		rf_d("FW Upgrade URL = %s \r\n", fw_url);
	http_resp_t *resp = NULL;
	httpc_cfg_t http_cfg;
	http_session_t http_session;
	memset(&http_cfg, 0x00, sizeof(httpc_cfg_t));


	if (cfg) {
#ifndef CONFIG_ENABLE_HTTPC_SECURE
		rf_d("HTTPC secure is disabled. Secure upgrade failed");
		return -WM_FAIL;
#else
		tls_lib_init();
		http_cfg.ctx = tls_create_client_context(cfg);
		if (!http_cfg.ctx)
			return -WM_FAIL;

		http_cfg.flags = TLS_ENABLE;
#endif /* CONFIG_ENABLE_HTTPC_SECURE */
	}

	http_cfg.retry_cnt = DEFAULT_RETRY_COUNT;

	status = httpc_get(fw_url, &http_session, &resp, &http_cfg);
	if (status != 0) {
#ifdef CONFIG_ENABLE_HTTPC_SECURE
		if (http_cfg.ctx)
			tls_purge_client_context(http_cfg.ctx);
#endif /* CONFIG_ENABLE_HTTPC_SECURE */
		rf_e("Unable to connect to server");
		return -WM_FAIL;
	}
	/* If response is not 200-OK, it means that some error was encountered.
	 * In that case, close the session and return error.
	 */
	if (resp->status_code != 200) {
		status = -WM_FAIL;
		goto httpc_fail;
	}

	if (!priv->fw_data_read_fn) {
		priv->fw_data_read_fn = __httpc_fw_data_read_fn;
		priv->session_priv_data = &http_session;
	}

	status = secure_upgrade(flash_comp_type, priv, p);

httpc_fail:
	http_close_session(&http_session);
#ifdef CONFIG_ENABLE_HTTPC_SECURE
		if (http_cfg.ctx)
			tls_purge_client_context(http_cfg.ctx);
#endif
	if (status != WM_SUCCESS)
		status = -WM_FAIL;
	return status;
}


int server_mode_secure_upgrade(enum flash_comp flash_comp_type,
		upgrade_struct_t *priv)
{
	if (!priv->fw_data_read_fn)
		return -WM_FAIL;
	struct partition_entry *p = __get_passive_partition(flash_comp_type);
	if (p == NULL) {
		rf_e("Failed to get passive partition");
		return -WM_FAIL;
	}
	int status = secure_upgrade(flash_comp_type, priv, p);
	if (status != WM_SUCCESS)
		status = -WM_FAIL;
	return status;
}
