/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* http_handlers.c
 * This file contains WSGI http handlers for the demo application
 */

#include <stdio.h>
#include <string.h>
#include <wmstdio.h>
#include <rfget.h>
#include <httpd.h>
#include <http_parse.h>
#include <http-strings.h>
#include <wm_net.h>
#include <json_parser.h>
#include <json_generator.h>
#include <app_framework.h>
#include <ezxml.h>

#include "app.h"
/* #include "cert_demo_http.h" */

#define TMP_BUF_SZ 5
char tmp_buffer[TMP_BUF_SZ];
static struct json_str jstr;



static int sys_anchor_start(struct json_str *jptr, char *buf, int len)
{
	json_str_init(jptr, buf, len);
	json_start_object(jptr);
	return 0;
}

static int sys_anchor_end(struct json_str *jptr)
{
	json_close_object(jptr);
	return 0;
}

int cert_demo_http_get_temp(httpd_request_t *req)
{
	char content[30];
	sys_anchor_start(&jstr, content, sizeof(content));
	json_set_val_int(&jstr, "temp", 26);
	sys_anchor_end(&jstr);
	return httpd_send_response(req, HTTP_RES_200, content,
			strlen(content), HTTP_CONTENT_JSON_STR);

	return 0;
}

/* End of GET http handler functions */

char zero_buf[256];
char scratch[512];

int cert_demo_http_data_handler(httpd_request_t *req)
{
	int err;

	/* Invoke the server provided handlers to process HTTP headers */
	err = httpd_parse_hdr_tags(req, req->sock, scratch, HTTPD_MAX_MESSAGE);
	if (err != WM_SUCCESS) {
		wmprintf("Unable to parse header tags");
	}

	/* Send 200 OK and plain/text */
	httpd_send_hdr_from_code(req->sock, 0, HTTP_CONTENT_PLAIN_TEXT);

	/* Keep sending the chunk in a loop until the other end closes
	 * connection. */
	while (1) {
		if (httpd_send_chunk(req->sock, zero_buf,
					sizeof(zero_buf)) != WM_SUCCESS)
			break;
	}
	return HTTPD_DONE;
}

struct httpd_wsgi_call g_cert_demo_http_handlers[] = {
	{"/demo/temp", HTTPD_DEFAULT_HDR_FLAGS, 0,
		cert_demo_http_get_temp, NULL, NULL, NULL},
	{"/data", HTTPD_DEFAULT_HDR_FLAGS, 0,
		cert_demo_http_data_handler, NULL, NULL, NULL},
};

static int g_cert_demo_handlers_no =
sizeof(g_cert_demo_http_handlers) / sizeof(struct httpd_wsgi_call);

void cert_demo_http_register_handlers()
{
	int rc;

	rc = httpd_register_wsgi_handlers(g_cert_demo_http_handlers,
				g_cert_demo_handlers_no);
	if (rc) {
		DEMO_APP_LOG
			("cert_demo_http: failed to register web handlers\r\n");
	}
}

/** Define additional handlers under /demo/ at this location. Create the
 * get/set handlers entries as appropriate.
 *
 * Please keep this array sorted in alphabetically ascending order of the
 * handler string.
 */
