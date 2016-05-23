/*
 * Copyright 2008-2015, Marvell International Ltd.
 * All Rights Reserved.
 *
 */

/** provisioning_web_handlers.c: Web Handlers for the provisioning interface
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <wmtypes.h>
#include <wmstdio.h>
#include <wm_os.h>
#include <wlan.h>
#include <httpd.h>
#include <http_parse.h>
#include <http-strings.h>
#include <cli_utils.h>		/* for get_mac() */
#include <provisioning.h>
#include <wm_net.h>
#include <json_parser.h>
#include <json_generator.h>
#include <hkdf-sha.h>
#include <mdev_aes.h>
#ifdef CONFIG_ENABLE_SCAN
#include <provisioning.h>
#endif
#include <app_framework.h>

#include "provisioning_int.h"
#include "curve25519-donna.h"
#include "wscan.h"

#define INVALID_SESSION_ID -1
#define ALLOC_ERROR -2
#define PARAM_ERROR -3
#define INTERNAL_ERROR -4

#define J_NAME_RAND_SIG "random_sig"
#define J_NAME_CPUB_KEY		"client_pub_key"
#define J_NAME_DPUB_KEY		"device_pub_key"
#define J_NAME_SESSION_ID	"session_id"
#define J_NAME_IV	"iv"
#define J_NAME_DATA	"data"
#define J_NAME_CHECKSUM	"checksum"

#define J_NAME_CONFIGURED       "configured"
#define J_NAME_STATUS           "status"
#define J_NAME_FAILURE          "failure"
#define J_NAME_FAILURE_CNT      "failure_cnt"
#define J_NAME_PROV_CLIENT_ACK  "prov_client_ack"

#define MAX_BUF_SIZE 300

#define HTTPD_JSON_SECURE_SESSION_RESPONSE "{\"iv\":\"%s\","\
"\"device_pub_key\":\"%s\",\"crypt_sig\":\"%s\",\"session_id\":%u}"
#define HTTPD_JSON_SECURE_ESP "{\"IV\":\"%s\","\
"\"data\":\"%s\",\"checksum\":\"%s\"}"
#define HTTPD_JSON_SECURE_SESSION_ERROR "{\"error-code\":%d}"
#define HTTPD_JSON_SECURE_SESSION_STATUS "{\"secure-session-status\":%s}"

#define CURVE_DATA_LEN 32
#define RANDOM_SIGN_LEN 64

/* Secure provisioning */
static char *buffer;
static char json_val[MAX_JSON_VAL_LEN + 1];
static struct json_str jstr;

typedef struct secure_session_cfg {
	uint8_t shared_secret[AES_BLOCK_SIZE];
	uint32_t session_id;
	char *pin;
	aes_t enc;
	SHA512Context sha;
	uint8_t checksum[SHA512HashSize];
} secure_session_cfg_t;

static secure_session_cfg_t s_cfg;

/* This pin is used to securely provision the device */
static char secure_pin[PROV_DEVICE_KEY_MAX_LENGTH];
static nw_save_cb_t nw_save_cb;

static int validate_session_id(httpd_request_t *req, char *buf, int size)
{
	uint32_t session_id;

	/* Validating if the request has a valid identifier */
	if (httpd_get_tag_from_url(req, J_NAME_SESSION_ID, buf, size)
	    != WM_SUCCESS) {
		return PARAM_ERROR;
	} else {
		session_id = atoi(buf);
		if (session_id != s_cfg.session_id) {
			return INVALID_SESSION_ID;
		}
	}
	return WM_SUCCESS;
}

static void secure_context_init(uint8_t *aes_IV)
{
	/* Initialize the context */
	memset(&s_cfg.enc, 0, sizeof(s_cfg.enc));
	memset(&s_cfg.sha, 0, sizeof(s_cfg.sha));
	memset(&s_cfg.checksum, 0, sizeof(s_cfg.checksum));
	SHA512Reset(&s_cfg.sha);

	/* Here making an assumtion that IV is already set to the
	   appropriate value */
	/* Encrypt the Random number with the generated shared secret */
	aes_drv_setkey(NULL, &s_cfg.enc, s_cfg.shared_secret, AES_BLOCK_SIZE,
		       aes_IV, AES_DECRYPTION, HW_AES_MODE_CTR);
}

static void encrypt_secure_data(void *inbuf, void *outbuf, int size)
{
	SHA512Input(&s_cfg.sha, (const uint8_t *)inbuf, size);
	aes_drv_encrypt(NULL, &s_cfg.enc, (uint8_t *)inbuf, outbuf, size);
}

static void decrypt_secure_data(void *inbuf, void *outbuf, int size)
{
	aes_drv_decrypt(NULL, &s_cfg.enc, (uint8_t *)inbuf, (uint8_t *) outbuf,
			size);
	SHA512Input(&s_cfg.sha, (const uint8_t *)outbuf, (unsigned int)
		    strlen(outbuf));
}

static void secure_context_deinit()
{
	SHA512Result(&s_cfg.sha, s_cfg.checksum);
}

/** This function can be used to construct an encrypted object
 * from the input data buffer
 *
 * \param [out] jobj containing encrypted json object
 * \param [in] input json object string to be encrypted
 * \param [in] size of json object string
 */
static void prepare_encrypted_json(struct json_str *jobj, char *data_buf,
			    int size, int max_buf_len)
{
	uint8_t aes_IV[AES_BLOCK_SIZE];

	/* Generate IV from random bytes */
	get_random_sequence(aes_IV, AES_BLOCK_SIZE);
	/* Encrypt the json data */
	secure_context_init(aes_IV);
	encrypt_secure_data(data_buf, (uint8_t *)data_buf, size);
	secure_context_deinit();

	memset(buffer, 0, MAX_BUF_SIZE);
	bin2hex((uint8_t *)data_buf, buffer,
		(unsigned int) size, MAX_BUF_SIZE);
	/* Prepare JSON object */
	/* Preparing the encrypted JSON object */
	json_str_init(jobj, data_buf, max_buf_len);
	json_start_object(jobj);
	json_set_val_str(jobj, J_NAME_DATA, buffer);

	memset(buffer, 0, MAX_BUF_SIZE);
	bin2hex(aes_IV, buffer,
		AES_BLOCK_SIZE, MAX_BUF_SIZE);
	json_set_val_str(jobj, J_NAME_IV, buffer);

	memset(buffer, 0, MAX_BUF_SIZE);
	bin2hex((uint8_t *) s_cfg.checksum, buffer,
		SHA512HashSize, MAX_BUF_SIZE);
	json_set_val_str(jobj, J_NAME_CHECKSUM, buffer);

	json_close_object(jobj);
}

static const char *get_resp_hdr(int error_code)
{
	switch (error_code) {
	case WM_SUCCESS:
		return HTTP_RES_200;
		break;
	case PARAM_ERROR:
		return HTTP_RES_400;
		break;
	case ALLOC_ERROR:
		return HTTP_RES_500;
		break;
	case INTERNAL_ERROR:
		return HTTP_RES_500;
		break;
	case INVALID_SESSION_ID:
		return HTTP_RES_400;
		break;
	default:
		return NULL;
	}
}

/** This function can be used to construct an decrypted json from the enrypted
 * json object
 *
 * \param [in|out] jobj containing encrypted json object as input. After
 * successful execution of this function jobj will be pointing to the decrypted
 * json object
 * \param [in] databuf buffer required to store the decrypted json object
 * \param [in] json_tokens_out tokens required by decrypted json object
 * \return [out] error_code : error code giving the reason of failure
 */
static int prepare_decrypted_json(jobj_t *jobj, char *data_buf,
				  int data_buf_size,
				  jsontok_t *json_tokens_out)
{
	/* Parse the JSON data */
	int error_code = WM_SUCCESS;
	int data_len;
	uint8_t *h2b_checksum;
	uint8_t aes_IV[AES_BLOCK_SIZE];

	h2b_checksum = os_mem_calloc(SHA512HashSize);
	if (h2b_checksum == NULL) {
		error_code = ALLOC_ERROR;
		goto out;
	}

	if (json_get_val_str(jobj, J_NAME_IV, buffer,
			     MAX_BUF_SIZE) != WM_SUCCESS) {
		error_code = PARAM_ERROR;
		goto out;
	}
	if (strlen(buffer) != (AES_BLOCK_SIZE * 2) ||
	    strlen(buffer) <= 0) {
		error_code = PARAM_ERROR;
		goto out;
	}
	hex2bin((uint8_t *)buffer, aes_IV, AES_BLOCK_SIZE);

	/* Verify the checksum */
	if (json_get_val_str(jobj, J_NAME_CHECKSUM, buffer,
			     MAX_BUF_SIZE) != WM_SUCCESS) {
		error_code = PARAM_ERROR;
		goto out;
	}
	if (strlen(buffer) != (SHA512HashSize * 2) ||
	    strlen(buffer) <= 0) {
		error_code = PARAM_ERROR;
		goto out;
	}
	hex2bin((uint8_t *)buffer, h2b_checksum, SHA512HashSize);

	if (json_get_val_str(jobj, J_NAME_DATA, buffer,
			     MAX_BUF_SIZE) != WM_SUCCESS) {
		error_code = PARAM_ERROR;
		goto out;
	}

	data_len = strlen(buffer);
	if (data_len <= 0) {
		error_code = PARAM_ERROR;
		goto out;
	}

	/* In-place hexadecimal to binary conversion of data */
	hex2bin((uint8_t *) buffer, (uint8_t *) buffer, MAX_BUF_SIZE);

	/* Decrypt the json data*/
	memset(data_buf, 0, data_buf_size);
	secure_context_init(aes_IV);
	decrypt_secure_data(buffer, data_buf, (data_len/2));
	secure_context_deinit();

	if (memcmp(s_cfg.checksum, h2b_checksum, SHA512HashSize) != 0) {
		prov_e("Error: decrypted object checksum mismatch\r\n");
		error_code = INTERNAL_ERROR;
		goto out;
	}

	error_code = json_init(jobj, json_tokens_out, NUM_TOKENS,
				data_buf, (int) strlen(data_buf));
	if (error_code != WM_SUCCESS) {
		error_code = INTERNAL_ERROR;
		goto out;
	}
out:
	if (h2b_checksum)
		os_mem_free(h2b_checksum);
	return error_code;
}

static int set_prov_info(httpd_request_t *req)
{
	int size = 512;
	int ack, error_code = WM_SUCCESS;
	int handle_client_ack = 0;
	char *content = NULL;
	content = os_mem_calloc(size);
	if (!content) {
		prov_e("Failed to allocate memory\r\n");
		return -WM_E_NOMEM;
	}

	error_code = validate_session_id(req, buffer, MAX_BUF_SIZE);
	if (error_code)
		goto out;

	error_code = httpd_get_data(req, content, size);
	/* check condition */
	if (error_code < 0) {
		prov_e("Failed to get post request data");
		return -WM_E_NOMEM;
	}
	/* Parse the JSON data */
	jsontok_t json_tokens[NUM_TOKENS];
	jobj_t jobj;
	error_code = json_init(&jobj, json_tokens, NUM_TOKENS,
			       content, req->body_nbytes);
	if (error_code != WM_SUCCESS) {
		error_code = INTERNAL_ERROR;
		goto out;
	}

	/* Decrypt the JSON object received */
	error_code = prepare_decrypted_json(&jobj, content, size,
					  json_tokens);
	if (error_code != WM_SUCCESS) {
		goto out;
	}

	if (json_get_val_int(&jobj, J_NAME_PROV_CLIENT_ACK,
			     &ack) == WM_SUCCESS) {
		handle_client_ack = 1;
	}

	httpd_purge_all_socket_data(req);
	snprintf(content, size, HTTPD_JSON_SUCCESS);
	prepare_encrypted_json(&jstr, content, strlen(content), size);
	httpd_send_response(req, HTTP_RES_200, content,
			    strlen(content), HTTP_CONTENT_JSON_STR);
	/* Delay added to send response to POST client_ack = 1 */
	os_thread_sleep(os_msec_to_ticks(1000));
	if (handle_client_ack)
		app_ctrl_notify_application_event(AF_EVT_PROV_CLIENT_DONE,
						  NULL);
	os_mem_free(content);
	return error_code;
out:
	if (error_code) {
		snprintf(buffer, MAX_BUF_SIZE, HTTPD_JSON_SECURE_SESSION_ERROR,
			 error_code);
	}

	prepare_encrypted_json(&jstr, buffer, strlen(buffer), size);
	httpd_purge_all_socket_data(req);
	httpd_send_response(req, get_resp_hdr(error_code),
					 buffer, strlen(buffer),
					 HTTP_CONTENT_JSON_STR);
	os_mem_free(content);
	return WM_SUCCESS;
}

static int get_prov_info(httpd_request_t *req)
{
	const int size = 512;
	int error_code = WM_SUCCESS, count = 0, mode;
	app_conn_status_t state;
	app_conn_failure_reason_t reason;
	char *content = NULL;

	content = os_mem_calloc(size);
	if (!content) {
		prov_e("Failed to allocate memory\r\n");
		return -WM_E_NOMEM;
	}

	error_code = validate_session_id(req, buffer, MAX_BUF_SIZE);
	if (error_code)
		goto out;

	json_str_init(&jstr, content, size);
	json_start_object(&jstr);
	app_get_connection_status(&state, &reason, &count);
	json_set_val_int(&jstr, J_NAME_STATUS, state);

#define auth_failed "auth_failed"
#define dhcp_failed "dhcp_failed"
#define network_failed "network_not_found"
#define unknown_reason "other"
	if (state == CONN_STATE_CONNECTING && state != CONN_STATE_CONNECTED) {
		if (reason == AUTH_FAILED) {
			json_set_val_str(&jstr, J_NAME_FAILURE, auth_failed);
			json_set_val_int(&jstr, J_NAME_FAILURE_CNT, count);
		}
		if (reason == DHCP_FAILED) {
			json_set_val_str(&jstr, J_NAME_FAILURE, dhcp_failed);
			json_set_val_int(&jstr, J_NAME_FAILURE_CNT, count);
		}
		if (reason == NETWORK_NOT_FOUND) {
			json_set_val_str(&jstr, J_NAME_FAILURE, network_failed);
			json_set_val_int(&jstr, J_NAME_FAILURE_CNT, count);
		}
		if (reason == -WM_FAIL) {
			json_set_val_str(&jstr, J_NAME_FAILURE, unknown_reason);
		}
	}
	mode = app_ctrl_getmode();
	json_set_val_int(&jstr, J_NAME_CONFIGURED, mode);
	json_close_object(&jstr);

	prepare_encrypted_json(&jstr, content, strlen(content), size);
out:
	if (error_code) {
		snprintf(content, size, HTTPD_JSON_SECURE_SESSION_ERROR,
			 error_code);
	}
	httpd_send_response(req, get_resp_hdr(error_code),
					 content, strlen(content),
					 HTTP_CONTENT_JSON_STR);
	os_mem_free(content);

	return WM_SUCCESS;
}

/*It is assumed that ip address(unsigned ip) will be in network order*/
static inline int validate_ip(unsigned ip)
{
	/* This will return -1 if one of the following case is true
	   1. x.x.x.255
	   2. x.x.x.0
	   3  0.x.x.x
	   4. 255.x.x.x
	   There is no further need to validate 0.0.0.0 and 255.255.255.255
	 */
	if (((ip >> 24) == 0XFF) ||
	    !(ip >> 24) || ((ip & 0xFF) == 0XFF) || !(ip & 0XFF))
		return -1;
	return 0;
}

static int parse_network_settings(jobj_t *jobj,
				  struct wlan_network *network)
{
	int security, channel, ret = WM_SUCCESS;
	char *tag = NULL;
	unsigned int len = 0;
	long addr;
	int json_int_val;

	memset(network, 0, sizeof(struct wlan_network));
	if (json_get_val_str(jobj, J_NAME_SSID, buffer, MAX_BUF_SIZE) !=
	    WM_SUCCESS)
		return -WM_FAIL;

	if (strlen(buffer) > IEEEtypes_SSID_SIZE || strlen(buffer) <= 0)
		return -WM_FAIL;

	strncpy(network->ssid, buffer, IEEEtypes_SSID_SIZE);

	ret = json_get_val_int(jobj, J_NAME_CHANNEL, &channel);
	if (ret == WM_SUCCESS)
		network->channel = channel;
	else
		network->channel = 0;

	if (json_get_val_int(jobj, J_NAME_SECURITY, &security) != WM_SUCCESS)
		return -WM_FAIL;

	if (is_valid_security(security) == -1)
		return -WM_FAIL;

	network->security.type = (enum wlan_security_type)security;

	if ((network->security.type == WLAN_SECURITY_WEP_OPEN) ||
	    (network->security.type == WLAN_SECURITY_WPA) ||
	    (network->security.type == WLAN_SECURITY_WPA2) ||
	    (network->security.type == WLAN_SECURITY_WPA_WPA2_MIXED) ||
	    (network->security.type == WLAN_SECURITY_WILDCARD)) {
		ret = json_get_val_str(jobj, J_NAME_KEY, buffer,
				       MAX_BUF_SIZE);
		if (ret != WM_SUCCESS && (network->security.type
					  != WLAN_SECURITY_WILDCARD))
			return -WM_FAIL;
		if (ret == WM_SUCCESS) {
			tag = buffer;
			len = strlen(tag);
		}
	}

	if (tag) {
		/* be sure to copy the null */
		strncpy(network->security.psk, tag, len + 1);
		network->security.psk_len = len;
	}
	if (json_get_val_int(jobj, J_NAME_IPTYPE, &json_int_val) != WM_SUCCESS)
		json_int_val = ADDR_TYPE_DHCP;	/* Default is dhcp */

	ret = WM_SUCCESS;
	if (json_int_val == ADDR_TYPE_STATIC) {
		/* assume failure */
		network->ip.ipv4.addr_type = ADDR_TYPE_STATIC;
		ret = -WM_FAIL;
		PARSE_IP_ELEMENT(jobj, J_NAME_IP, network->ip.ipv4.address);
		if (validate_ip(network->ip.ipv4.address) != WM_SUCCESS)
			goto done;
		PARSE_IP_ELEMENT(jobj, J_NAME_GW, network->ip.ipv4.gw);
		if (validate_ip(network->ip.ipv4.gw) != WM_SUCCESS)
			goto done;
		PARSE_IP_ELEMENT(jobj, J_NAME_NETMASK,
				 network->ip.ipv4.netmask);
		if (json_get_val_str(jobj, J_NAME_DNS1, buffer,
				     MAX_BUF_SIZE) == WM_SUCCESS) {
			network->ip.ipv4.dns1 = inet_addr(buffer);
		}
		if (json_get_val_str(jobj, J_NAME_DNS2, buffer,
				     MAX_BUF_SIZE) == WM_SUCCESS) {
			network->ip.ipv4.dns2 = inet_addr(buffer);
		}
		ret = WM_SUCCESS;
	} else if (json_int_val == ADDR_TYPE_LLA) {
		network->ip.ipv4.addr_type = ADDR_TYPE_LLA;

	} else {
		network->ip.ipv4.addr_type = ADDR_TYPE_DHCP;
	}

done:
	return ret;
}

static int set_prov_network_handler(httpd_request_t *req)
{
	const int size = 512;
	int error_code = WM_SUCCESS;
	char *content = NULL;
	struct wlan_network current_net;

	memset(&current_net, 0, sizeof(struct wlan_network));
	content = os_mem_calloc(size);
	if (!content) {
		prov_e("Failed to allocate response content");
		return -WM_E_NOMEM;
	}

	error_code = validate_session_id(req, buffer, MAX_BUF_SIZE);
	if (error_code)
		goto out;

	error_code = httpd_get_data(req, content, size);
	if (error_code < 0) {
		prov_e("Failed to get post request data");
		error_code = INTERNAL_ERROR;
		goto out;
	}
	/* Parse the JSON data */
	jsontok_t json_tokens[NUM_TOKENS];
	jobj_t jobj;
	error_code = json_init(&jobj, json_tokens, NUM_TOKENS,
			       content, req->body_nbytes);
	if (error_code != WM_SUCCESS) {
		error_code = INTERNAL_ERROR;
		goto out;
	}

	/* Decrypt the JSON object received */
	error_code = prepare_decrypted_json(&jobj, content, size,
					  json_tokens);
	if (error_code != WM_SUCCESS) {
		goto out;
	}

	error_code = parse_network_settings(&jobj, &current_net);
	if (error_code) {
		error_code = INTERNAL_ERROR;
		goto out;
	}

	/* Send response before setting the network */
	snprintf(content, size, HTTPD_JSON_SUCCESS);
	httpd_purge_all_socket_data(req);
	prepare_encrypted_json(&jstr, content, strlen(content), size);
	httpd_send_response(req, HTTP_RES_200,
			    content, strlen(content),
			    HTTP_CONTENT_JSON_STR);

	/* Wait on http thread to send response */
	os_thread_sleep(os_msec_to_ticks(1000));

	error_code = nw_save_cb(&current_net);
	if (error_code) {
		error_code = INTERNAL_ERROR;
		goto out;
	}

	os_mem_free(content);
	return error_code;
out:
	if (error_code) {
		snprintf(content, size, HTTPD_JSON_SECURE_SESSION_ERROR,
			 error_code);
	}

	prepare_encrypted_json(&jstr, content, strlen(content), size);
	httpd_purge_all_socket_data(req);
	httpd_send_response(req, get_resp_hdr(error_code),
			    content, strlen(content),
			    HTTP_CONTENT_JSON_STR);
	os_mem_free(content);
	return WM_SUCCESS;
}

static int get_client_data(jobj_t *jobj, uint8_t *client_public_key,
			   uint8_t *random_number)
{
	/* Get the random rumber from the request */
	if (json_get_val_str(jobj, J_NAME_RAND_SIG, buffer,
			     MAX_BUF_SIZE) != WM_SUCCESS) {
		return -WM_FAIL;
	}

	if (strlen(buffer) != (RANDOM_SIGN_LEN * 2) ||
	    strlen(buffer) <= 0) {
		return -WM_FAIL;
	}
	hex2bin((uint8_t *)buffer, random_number, RANDOM_SIGN_LEN);

	/* Get the client's Pulic Key */
	if (json_get_val_str(jobj, J_NAME_CPUB_KEY, buffer,
			     MAX_BUF_SIZE) != WM_SUCCESS) {
		return -WM_FAIL;
	}

	if (strlen(buffer) != (CURVE_DATA_LEN * 2) ||
	    strlen(buffer) <= 0) {
		return -WM_FAIL;
	}
	hex2bin((uint8_t *)buffer, client_public_key, CURVE_DATA_LEN);

	return WM_SUCCESS;
}

static int compute_shared_secret(uint8_t *client_public_key,
				 uint8_t *device_public_key,
				 uint8_t *shared_secret)
{
	uint8_t *device_private_key = NULL;
	uint8_t *tmp_secret = NULL, *tmp_secret_hash = NULL, *pin_hash = NULL;
	int ret = WM_SUCCESS, i = 0;

	/* Initialize the SHA512 context */
	memset(&s_cfg.sha, 0, sizeof(s_cfg.sha));

	device_private_key = os_mem_calloc(CURVE_DATA_LEN);
	if (device_private_key == NULL) {
		prov_e("Failed to allocate memory\r\n");
		ret = -WM_FAIL;
		goto out;
	}

	/* Generating a Private Key using random bytes */
	get_random_sequence(device_private_key, CURVE_DATA_LEN);

	/* Deriving the Public Key using curve25519 */
	curve25519_donna(device_public_key, device_private_key, NULL);

	/* Memory allocations required */
	tmp_secret = os_mem_calloc(CURVE_DATA_LEN);
	tmp_secret_hash = os_mem_calloc(SHA512HashSize);
	pin_hash = os_mem_calloc(SHA512HashSize);

	if (tmp_secret == NULL || tmp_secret_hash == NULL ||
	    pin_hash == NULL) {
		prov_e("Failed to allocate memory\r\n");
		ret = -WM_FAIL;
		goto out;
	}
	/* Compute the tempeory secret using curve25519 secret */
	curve25519_donna(tmp_secret, device_private_key, client_public_key);

	/* Taking SHA512 of pin */
	SHA512Reset(&s_cfg.sha);
	SHA512Input(&s_cfg.sha, (const uint8_t *)s_cfg.pin, strlen(s_cfg.pin));
	SHA512Result(&s_cfg.sha, pin_hash);

	/* Taking SHA512 of tmp_secret */
	SHA512Reset(&s_cfg.sha);
	SHA512Input(&s_cfg.sha, tmp_secret, CURVE_DATA_LEN);
	SHA512Result(&s_cfg.sha, tmp_secret_hash);

	/* XOR the pin Hash and Generated shared secret */
	for (i = 0; i < AES_BLOCK_SIZE; i++) {
		shared_secret[i] =  (tmp_secret_hash[i] ^ pin_hash[i]);
	}
out:
	if (device_private_key)
		os_mem_free(device_private_key);
	if (tmp_secret)
		os_mem_free(tmp_secret);
	if (tmp_secret_hash)
		os_mem_free(tmp_secret_hash);
	if (pin_hash)
		os_mem_free(pin_hash);
	return ret;
}

/** This function computes the shared secret  and constructs the response
 * with the encrypted random number
 *
 * \param [in] jobj containing input json object from controller
 * \param [out] content output buffer containing json response
 * \param [in] size maximum content length
 */
static int process_secure_session_request(jobj_t *jobj, char *content, int size)
{
	int error_code = WM_SUCCESS;
	uint8_t *client_public_key = NULL, *device_public_key = NULL;
	uint8_t *random_number = NULL;
	uint8_t aes_IV[AES_BLOCK_SIZE];

	/* Initialize the AES context */
	memset(&s_cfg.enc, 0, sizeof(s_cfg.enc));

	client_public_key = os_mem_calloc(CURVE_DATA_LEN);
	random_number = os_mem_calloc(RANDOM_SIGN_LEN);
	device_public_key = os_mem_calloc(CURVE_DATA_LEN);
	if (client_public_key == NULL || random_number == NULL ||
	    device_public_key == NULL) {
		prov_e("Failed to allocate memory\r\n");
		error_code = ALLOC_ERROR;
		goto out;
	}

	/* Retrieve the client's public key and random signature from
	   the JSON */
	if (get_client_data(jobj, client_public_key, random_number)
	    != WM_SUCCESS) {
		error_code = PARAM_ERROR;
		goto out;
	}

	/* Randomly generate the IV */
	get_random_sequence(aes_IV, AES_BLOCK_SIZE);

	/* Generate Shared Secret */
	if (compute_shared_secret(client_public_key, device_public_key,
				  s_cfg.shared_secret)
	    != WM_SUCCESS) {
		error_code = ALLOC_ERROR;
		goto out;
	}

	/* Encrypt the json data */
	secure_context_init(aes_IV);
	encrypt_secure_data(random_number, (uint8_t *)random_number,
			    RANDOM_SIGN_LEN);
	secure_context_deinit();

	/* Generating session identifier */
	s_cfg.session_id = rand();

	/* Preparing the encrypted JSON object */
	json_str_init(&jstr, content, size);
	json_start_object(&jstr);

	json_set_val_int(&jstr, J_NAME_SESSION_ID, s_cfg.session_id);

	memset(buffer, 0, MAX_BUF_SIZE);
	bin2hex(device_public_key, buffer,
		CURVE_DATA_LEN, MAX_BUF_SIZE);
	json_set_val_str(&jstr, J_NAME_DPUB_KEY, buffer);

	memset(buffer, 0, MAX_BUF_SIZE);
	bin2hex(aes_IV, buffer,
		AES_BLOCK_SIZE, MAX_BUF_SIZE);
	json_set_val_str(&jstr, J_NAME_IV, buffer);

	memset(buffer, 0, MAX_BUF_SIZE);
	bin2hex((uint8_t *)random_number, buffer, (unsigned int)
		RANDOM_SIGN_LEN, MAX_BUF_SIZE);
	json_set_val_str(&jstr, J_NAME_DATA, buffer);

	memset(buffer, 0, MAX_BUF_SIZE);
	bin2hex((uint8_t *) s_cfg.checksum, buffer,
		SHA512HashSize, MAX_BUF_SIZE);
	json_set_val_str(&jstr, J_NAME_CHECKSUM, buffer);

	json_close_object(&jstr);

out:
	/* Free all the allocated memory */
	if (client_public_key)
		os_mem_free(client_public_key);
	if (device_public_key)
		os_mem_free(device_public_key);
	if (random_number)
		os_mem_free(random_number);
	return error_code;
}

static int set_secure_session_handler(httpd_request_t *req)
{
	const int size = 512;
	char *content = NULL;
	int error_code = WM_SUCCESS;

	content = os_mem_calloc(size);
	if (!content) {
		return -WM_E_NOMEM;
	}

	error_code = httpd_get_data(req, content, size);
	if (error_code < 0) {
		prov_e("Failed to get post request data");
		error_code = INTERNAL_ERROR;
		goto exit;
	}

	/* Parse the JSON data */
	jsontok_t json_tokens[NUM_TOKENS];
	jobj_t jobj;
	error_code = json_init(&jobj, json_tokens, NUM_TOKENS,
			       content, req->body_nbytes);
	if (error_code != WM_SUCCESS) {
		prov_e("Failed to initialize json object for request content");
		error_code = INTERNAL_ERROR;
		goto exit;
	}

	error_code = process_secure_session_request(&jobj, content, size);
exit:
	/* Send response before setting the network */
	if (error_code) {
		snprintf(content, size, HTTPD_JSON_SECURE_SESSION_ERROR,
			 error_code);
	}
	httpd_purge_all_socket_data(req);
	httpd_send_response(req, get_resp_hdr(error_code),
			    content, strlen(content),
			    HTTP_CONTENT_JSON_STR);
	os_mem_free(content);
	return WM_SUCCESS;
}

static int get_secure_session_handler(httpd_request_t *req)
{
	const int size = 100;
	int ret;
	char *content;

	content = os_mem_calloc(size);

	if (!content) {
		prov_e("Failed to allocate response content");
		return -WM_E_NOMEM;
	}

	if (s_cfg.session_id != 0) {
		snprintf(content, size, HTTPD_JSON_SECURE_SESSION_STATUS,
			 "true");
	} else {
		snprintf(content, size, HTTPD_JSON_SECURE_SESSION_STATUS,
			 "false");
	}

	ret = httpd_send_response(req, HTTP_RES_200, content, strlen(content),
				  HTTP_CONTENT_JSON_STR);

	os_mem_free(content);
	return ret;
}

#ifdef CONFIG_ENABLE_SCAN
static int send_scan_results(httpd_request_t *req, char *content, int size)
{
	struct wlan_scan_result *scan_result;
	struct json_str jstr, *jptr;
	int scan_results_count, bcn_nf_last;
	int i, mode, error_code = WM_SUCCESS;
	unsigned char count;
	char index_str[3], *ciphertxt;
	char tempstr[50] = { 0, };

	error_code = WM_SUCCESS;
	scan_results_count = wscan_get_scan_count();
	if (scan_results_count <= 0) {
		/* Return Failure */
		return INTERNAL_ERROR;
	}

	if (wscan_get_lock_scan_results() != WM_SUCCESS) {
		return INTERNAL_ERROR;
	}

	ciphertxt = os_mem_calloc(size);
	if (!ciphertxt) {
		/* Release the scan results lock */
		wscan_put_lock_scan_results();
		return ALLOC_ERROR;
	}

	bcn_nf_last = wlan_get_current_nf();

	jptr = &jstr;

	json_str_init(jptr, content, size);
	json_start_object(jptr);
	json_push_array_object(jptr, J_NAME_NETWORKS);

	for (i = 0, count = 0; i < scan_results_count; i++) {
		error_code = wscan_get_scan_result(&scan_result, i);
		if (error_code != WM_SUCCESS) {
			/* Release the scan results lock */
			wscan_put_lock_scan_results();
			os_mem_free(ciphertxt);
			return INTERNAL_ERROR;
		}

		if (scan_result->ssid[0] == 0)
			continue;
		else
			/* Found valid ssid increment count */
			count++;

		mode = 0;
		/* Our preference is in the following order: wpa2, wpa, wep */
		if (scan_result->wpa2 && scan_result->wpa)
			mode = 5;
		else if (scan_result->wpa2)
			mode = 4;
		else if (scan_result->wpa)
			mode = 3;
		else if (scan_result->wep)
			mode = 1;
		snprintf(tempstr, sizeof(tempstr),
			 "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
			 scan_result->bssid[0], scan_result->bssid[1],
			 scan_result->bssid[2], scan_result->bssid[3],
			 scan_result->bssid[4], scan_result->bssid[5]);
		snprintf(index_str, 2, "%d", i);

		if (count > 1) {
			json_str_init(jptr, content, HTTPD_MAX_MESSAGE);
			json_start_object(jptr);
		}

		json_start_array(jptr);
		json_set_array_str(jptr, scan_result->ssid);
		json_set_array_str(jptr, tempstr);
		json_set_array_int(jptr, mode);
		json_set_array_int(jptr, scan_result->channel);
		json_set_array_int(jptr, -(scan_result->rssi));
		json_set_array_int(jptr, bcn_nf_last);
		json_close_array(jptr);
		if (count > 1) {
			/* This will overwrite '{' character set by
			 * json_object_init call with ','for array objects
			 * after first network.
			 */
			*content = ',';
		}

		if ((scan_results_count - i) != 1) {

			encrypt_secure_data(content, (uint8_t *)ciphertxt,
					    strlen(content));
			memset(buffer, 0, MAX_BUF_SIZE);
			bin2hex((uint8_t *)ciphertxt, buffer,
				(unsigned int) strlen(content),
				(unsigned int) MAX_BUF_SIZE);
			/* Check out len */
			httpd_send_chunk(req->sock, buffer,
					 strlen(buffer));
		}
	}

	json_pop_array_object(jptr);
	json_close_object(jptr);

	encrypt_secure_data(content, (uint8_t *)ciphertxt,
			    strlen(content));
	memset(buffer, 0, MAX_BUF_SIZE);
	bin2hex((uint8_t *)ciphertxt, buffer,
		(unsigned int) strlen(content),
		(unsigned int) MAX_BUF_SIZE);

	httpd_send_chunk(req->sock, buffer, strlen(buffer));

	os_mem_free(ciphertxt);
	/* Release the scan results lock */
	wscan_put_lock_scan_results();
	return WM_SUCCESS;
}

static int wsgi_handler_get_scan_results(httpd_request_t *req)
{
	int size = 512;
	int error_code = WM_SUCCESS;
	char *content;
	uint8_t aes_IV[AES_BLOCK_SIZE];
	content = os_mem_calloc(size);
	if (!content) {
		prov_e("Failed to allocate memory\r\n");
		return -WM_E_NOMEM;
	}

	error_code = validate_session_id(req, buffer, MAX_BUF_SIZE);
	if (error_code) {
		error_code = httpd_send_response(req, get_resp_hdr(error_code),
						 content, strlen(content),
						 HTTP_CONTENT_JSON_STR);
		return error_code;
	}

	error_code = httpd_send_response_header(req, HTTP_RES_200, 0,
						HTTP_CONTENT_JSON_STR);

	/* Generate IV from random bytes */
	get_random_sequence(aes_IV, AES_BLOCK_SIZE);
	/* Send the IV */
	memset(buffer, 0, MAX_BUF_SIZE);
	bin2hex(aes_IV, buffer, AES_BLOCK_SIZE,
		MAX_BUF_SIZE);
	snprintf(content, size, "{\"%s\":\"%s\", \"%s\":\"",
		 J_NAME_IV, buffer, J_NAME_DATA);
	httpd_send_chunk(req->sock, content, strlen(content));

	/* Send encrypted json data */
	secure_context_init(aes_IV);
	error_code = send_scan_results(req, content, size);
	secure_context_deinit();

	if (error_code != WM_SUCCESS) {
		goto out;
	}
	/* Send the checksum */
	memset(buffer, 0, MAX_BUF_SIZE);
	bin2hex((uint8_t *) s_cfg.checksum, buffer,
		SHA512HashSize, MAX_BUF_SIZE);
	snprintf(content, size, "\", \"%s\":\"%s\"}",
		 J_NAME_CHECKSUM, buffer);
out:
	if (error_code) {
		snprintf(content, size, HTTPD_JSON_SECURE_SESSION_ERROR,
			 error_code);
	}
	httpd_send_chunk(req->sock, content, strlen(content));
	httpd_send_chunk(req->sock, NULL, 0);
	if (content)
		os_mem_free(content);
	return WM_SUCCESS;
}
#endif

static struct httpd_wsgi_call secure_provisioning_handlers[] = {
	{"/prov/secure-session", HTTPD_DEFAULT_HDR_FLAGS, 0,
		get_secure_session_handler, set_secure_session_handler,
		NULL, NULL},
	{"/prov/network", HTTPD_DEFAULT_HDR_FLAGS, 0,
		NULL, set_prov_network_handler, NULL, NULL},
#ifdef CONFIG_ENABLE_SCAN
	{"/prov/scan", HTTPD_DEFAULT_HDR_FLAGS, 0,
		wsgi_handler_get_scan_results, NULL, NULL, NULL},
#endif
	{"/prov/net-info", HTTPD_DEFAULT_HDR_FLAGS, 0,
		get_prov_info, set_prov_info, NULL, NULL},
};

static int secure_provisioning_handlers_no =
	sizeof(secure_provisioning_handlers) / sizeof(struct httpd_wsgi_call);

int register_secure_provisioning_web_handlers(uint8_t *key, int len,
					      nw_save_cb_t cb)
{
	if (!len) {
		prov_e("Invalid secure pin length");
		return -WM_FAIL;
	}

	memcpy(secure_pin, key, len);
	nw_save_cb = cb;
	/* Allocating an initial buffer which is required throughout the
	   Provisioning Service */
	s_cfg.pin = secure_pin;

	buffer = os_mem_alloc(MAX_BUF_SIZE);
	if (!buffer) {
		prov_e("Failed to allocate memory");
		return -WM_FAIL;
	}
	if (httpd_register_wsgi_handlers(secure_provisioning_handlers,
				secure_provisioning_handlers_no)
			!= WM_SUCCESS) {
		prov_e("Failed to register web hanlders");
		return -WM_FAIL;
	}
	return WM_SUCCESS;
}

void unregister_secure_provisioning_web_handlers(void)
{
	memset(secure_pin, 0, sizeof(secure_pin));
	/* De-allocating the goal buffer allocated */
	if (buffer)
		os_mem_free(buffer);
	httpd_unregister_wsgi_handlers(secure_provisioning_handlers,
			secure_provisioning_handlers_no);
	return;
}
