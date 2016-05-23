/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Cloud Utils
 *
 * Summary:
 *
 * Description:
 *
 */

#include <psm-v2.h>
#include <psm.h>
#include <aws_utils.h>
#include <wmsdk.h>
#include <app_framework.h>

#define AWS_PUB_CERT  "cloud.certificate"
#define AWS_PRIV_KEY  "cloud.private_key"
#define AWS_REGION    "cloud.region"
#define AWS_THING     "cloud.thing"
#define J_CERT           "cert"
#define J_KEY            "key"
#define J_REGION         "region"
#define J_THING          "thing"

int read_aws_certificate(char *cert, unsigned cert_len)
{
	int i, j, n;
	char *buf = os_mem_calloc(AWS_PUB_CERT_SIZE);

	n = psm_get_variable(app_psm_hnd, AWS_PUB_CERT, buf,
			     AWS_PUB_CERT_SIZE);
	if (n <= 0 || n > cert_len) {
		os_mem_free(buf);
		return -WM_FAIL;
	}
	for (i = 0, j = 0; i < n; i++) {
		if (buf[i] == 0x5c && buf[i + 1] == 0x6e) {
			cert[j] = 0x0a;
			i++;
		} else {
			cert[j] = buf[i];
		}
		j++;
	}

	os_mem_free(buf);
	return WM_SUCCESS;
}

int read_aws_key(char *key, unsigned key_len)
{
	int i, j, n;
	char *buf = os_mem_calloc(AWS_PUB_CERT_SIZE);
	n = psm_get_variable(app_psm_hnd, AWS_PRIV_KEY, buf,
			     AWS_PRIV_KEY_SIZE);
	if (n <= 0 || n > key_len) {
		os_mem_free(buf);
		return -WM_FAIL;
	}
	for (i = 0, j = 0; i < n; i++) {
		if (buf[i] == 0x5c && buf[i + 1] == 0x6e) {
			key[j] = 0x0a;
			i++;
		} else {
			key[j] = buf[i];
		}
		j++;
	}
	os_mem_free(buf);
	return WM_SUCCESS;
}

int read_aws_region(char *region, unsigned region_len)
{
	int n;
	n = psm_get_variable(app_psm_hnd, AWS_REGION, region,
			     AWS_MAX_REGION_SIZE);
	if (n <= 0 || n > region_len)
		return -WM_FAIL;
	return WM_SUCCESS;
}

int read_aws_thing(char *thing, unsigned thing_len)
{
	int n;
	n = psm_get_variable(app_psm_hnd, AWS_THING, thing,
			     AWS_MAX_THING_SIZE);
	if (n <= 0 || n > thing_len)
		return -WM_FAIL;
	return WM_SUCCESS;
}


#define REQUEST_BUFSIZE 2096
#define CERT_BUFSIZE 2048
#define JSON_NUM_TOKENS 5
int aws_iot_set_cert_info(httpd_request_t *req)
{
	int ret;
	char *cert = NULL;
	char *reqbuf = NULL;
	char *response = HTTP_RES_400;
	char *status_code = HTTPD_JSON_ERROR;

	cert = os_mem_alloc(CERT_BUFSIZE);
	if (!cert) {
		wmprintf("Failed to allocate memory");
		goto out;
	}
	reqbuf = os_mem_alloc(REQUEST_BUFSIZE);
	if (!reqbuf) {
		wmprintf("Failed to allocate memory");
		goto out;
	}

	ret = httpd_get_data(req, reqbuf, REQUEST_BUFSIZE);
	if (ret < 0) {
		wmprintf("Failed to get request data");
		goto out;
	}

	jsontok_t json_tokens[JSON_NUM_TOKENS];
	jobj_t jobj;
	ret = json_init(&jobj, json_tokens, JSON_NUM_TOKENS,
			reqbuf, req->body_nbytes);
	if (ret != WM_SUCCESS)
		goto out;
	if (json_get_val_str(&jobj, J_CERT, cert, CERT_BUFSIZE) ==
	    WM_SUCCESS) {
		if (!strcmp(cert, NULL))
			goto out;
		if (psm_set_variable(app_psm_hnd, AWS_PUB_CERT,
				     cert, strlen(cert)) != WM_SUCCESS)
			wmprintf("Error: Could not set public certificate\r\n");
	} else {
		goto out;
	}

	response = HTTP_RES_200;
	status_code = HTTPD_JSON_SUCCESS;
out:
	ret = httpd_send_response(req, response, status_code,
				  strlen(status_code),
				  HTTP_CONTENT_JSON_STR);
	if (cert)
		os_mem_free(cert);
	if (reqbuf)
		os_mem_free(reqbuf);
	return ret;
}

int aws_iot_get_cert_info(httpd_request_t *req)
{
	httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_SUCCESS,
				  strlen(HTTPD_JSON_SUCCESS),
				  HTTP_CONTENT_JSON_STR);
	return WM_SUCCESS;
}

#define KEY_BUFSIZE 2048
int aws_iot_set_key_info(httpd_request_t *req)
{
	int ret;
	char *key = NULL;
	char *reqbuf = NULL;
	char *response = HTTP_RES_400;
	char *status_code = HTTPD_JSON_ERROR;

	key = os_mem_alloc(KEY_BUFSIZE);
	if (!key) {
		wmprintf("Failed to allocate memory");
		goto out;
	}
	reqbuf = os_mem_alloc(REQUEST_BUFSIZE);
	if (!reqbuf) {
		wmprintf("Failed to allocate memory");
		goto out;
	}

	ret = httpd_get_data(req, reqbuf, REQUEST_BUFSIZE);
	if (ret < 0) {
		wmprintf("Failed to get request data");
		goto out;
	}

	jsontok_t json_tokens[JSON_NUM_TOKENS];
	jobj_t jobj;
	ret = json_init(&jobj, json_tokens, JSON_NUM_TOKENS,
			reqbuf, req->body_nbytes);
	if (ret != WM_SUCCESS)
		goto out;

	if (json_get_val_str(&jobj, J_KEY, key, KEY_BUFSIZE) ==
	    WM_SUCCESS) {
		if (!strcmp(key, NULL))
			goto out;
		if (psm_set_variable(app_psm_hnd, AWS_PRIV_KEY,
				     key, strlen(key)) != WM_SUCCESS)
			wmprintf("Error: Could not set private key\r\n");
	} else {
		goto out;
	}

	response = HTTP_RES_200;
	status_code = HTTPD_JSON_SUCCESS;
out:
	ret = httpd_send_response(req, response, status_code,
				  strlen(status_code),
				  HTTP_CONTENT_JSON_STR);
	if (key)
		os_mem_free(key);
	if (reqbuf)
		os_mem_free(reqbuf);
	return ret;
}

int aws_iot_get_key_info(httpd_request_t *req)
{
	httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_SUCCESS,
				  strlen(HTTPD_JSON_SUCCESS),
				  HTTP_CONTENT_JSON_STR);
	return WM_SUCCESS;
}

#define REGION_BUFSIZE 128
int aws_iot_set_region_info(httpd_request_t *req)
{
	int ret;
	char *region = NULL;
	char *reqbuf = NULL;
	char *response = HTTP_RES_400;
	char *status_code = HTTPD_JSON_ERROR;

	region = os_mem_alloc(REGION_BUFSIZE);
	if (!region) {
		wmprintf("Failed to allocate memory");
		goto out;
	}
	reqbuf = os_mem_alloc(REQUEST_BUFSIZE);
	if (!reqbuf) {
		wmprintf("Failed to allocate memory");
		goto out;
	}

	ret = httpd_get_data(req, reqbuf, REQUEST_BUFSIZE);
	if (ret < 0) {
		wmprintf("Failed to get request data");
		goto out;
	}

	jsontok_t json_tokens[JSON_NUM_TOKENS];
	jobj_t jobj;
	ret = json_init(&jobj, json_tokens, JSON_NUM_TOKENS,
			reqbuf, req->body_nbytes);
	if (ret != WM_SUCCESS)
		goto out;

	if (json_get_val_str(&jobj, J_REGION, region, REGION_BUFSIZE) ==
	    WM_SUCCESS) {
		if (!strcmp(region, NULL))
			goto out;
		if (psm_set_variable(app_psm_hnd, AWS_REGION,
				     region, strlen(region)) != WM_SUCCESS)
			wmprintf("Error: Could not set region\r\n");
	} else {
		goto out;
	}

	response = HTTP_RES_200;
	status_code = HTTPD_JSON_SUCCESS;
out:
	ret = httpd_send_response(req, response, status_code,
				  strlen(status_code),
				  HTTP_CONTENT_JSON_STR);
	if (region)
		os_mem_free(region);
	if (reqbuf)
		os_mem_free(reqbuf);
	return ret;
}

int aws_iot_get_region_info(httpd_request_t *req)
{
	httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_SUCCESS,
				  strlen(HTTPD_JSON_SUCCESS),
				  HTTP_CONTENT_JSON_STR);
	return WM_SUCCESS;
}

#define THING_BUFSIZE 128
int aws_iot_set_thing_info(httpd_request_t *req)
{
	int ret;
	char *thing = NULL;
	char *reqbuf = NULL;
	char *response = HTTP_RES_400;
	char *status_code = HTTPD_JSON_ERROR;

	thing = os_mem_alloc(THING_BUFSIZE);
	if (!thing) {
		wmprintf("Failed to allocate memory");
		goto out;
	}
	reqbuf = os_mem_alloc(THING_BUFSIZE);
	if (!reqbuf) {
		wmprintf("Failed to allocate memory");
		goto out;
	}

	ret = httpd_get_data(req, reqbuf, THING_BUFSIZE);
	if (ret < 0) {
		wmprintf("Failed to get request data");
		goto out;
	}

	jsontok_t json_tokens[JSON_NUM_TOKENS];
	jobj_t jobj;
	ret = json_init(&jobj, json_tokens, JSON_NUM_TOKENS,
			reqbuf, req->body_nbytes);
	if (ret != WM_SUCCESS)
		goto out;

	if (json_get_val_str(&jobj, J_THING, thing, THING_BUFSIZE) ==
	    WM_SUCCESS) {
		if (!strcmp(thing, NULL))
			goto out;
		if (psm_set_variable(app_psm_hnd, AWS_THING,
				     thing, strlen(thing)) != WM_SUCCESS)
			wmprintf("Error: Could not set region\r\n");
	} else {
		goto out;
	}

	response = HTTP_RES_200;
	status_code = HTTPD_JSON_SUCCESS;
out:
	ret = httpd_send_response(req, response, status_code,
				  strlen(status_code),
				  HTTP_CONTENT_JSON_STR);
	if (thing)
		os_mem_free(thing);
	if (reqbuf)
		os_mem_free(reqbuf);
	return ret;
}

int aws_iot_get_thing_info(httpd_request_t *req)
{
	httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_SUCCESS,
				  strlen(HTTPD_JSON_SUCCESS),
				  HTTP_CONTENT_JSON_STR);
	return WM_SUCCESS;
}

struct httpd_wsgi_call aws_iot_http_handlers[] = {
	{"/aws_iot/pubCert", HTTPD_DEFAULT_HDR_FLAGS, 0, aws_iot_get_cert_info,
	 aws_iot_set_cert_info, NULL, NULL},
	{"/aws_iot/privKey", HTTPD_DEFAULT_HDR_FLAGS, 0, aws_iot_get_key_info,
	 aws_iot_set_key_info, NULL, NULL},
	{"/aws_iot/region", HTTPD_DEFAULT_HDR_FLAGS, 0, aws_iot_get_region_info,
	 aws_iot_set_region_info, NULL, NULL},
	{"/aws_iot/thing", HTTPD_DEFAULT_HDR_FLAGS, 0, aws_iot_get_thing_info,
	 aws_iot_set_thing_info, NULL, NULL},
};

static int aws_iot_handlers_no =
	sizeof(aws_iot_http_handlers) / sizeof(struct httpd_wsgi_call);

int enable_aws_config_support()
{
	wm_wlan_set_httpd_handlers(aws_iot_http_handlers, aws_iot_handlers_no);
	return WM_SUCCESS;
}
