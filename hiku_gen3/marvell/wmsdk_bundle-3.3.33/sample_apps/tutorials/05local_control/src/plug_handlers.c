/*
 *  Copyright (C) 2008-2016 Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <wmstdio.h>
#include <stdio.h>
#include <string.h>
#include <httpd.h>
#include <json_parser.h>

bool plug_state = true;

static int plug_http_get_power_state(httpd_request_t *req)
{
	char content[30];
	snprintf(content, sizeof(content), "{ \"power\": %d }",
		 plug_state ? 1 : 0);
	return httpd_send_response(req, HTTP_RES_200, content,
				   strlen(content), HTTP_CONTENT_JSON_STR);
}

#define NUM_TOKENS 6
static int plug_http_set_power_state(httpd_request_t *req)
{
	char plug_data[30];

	int ret = httpd_get_data(req, plug_data, sizeof(plug_data));
	if (ret < 0) {
		wmprintf("Failed to get post request data");
		return ret;
	}

	/* Parse the JSON data */
	jsontok_t json_tokens[NUM_TOKENS];
	jobj_t jobj;
	ret = json_init(&jobj, json_tokens, NUM_TOKENS, plug_data,
			req->body_nbytes);
	if (ret != WM_SUCCESS)
		return -WM_FAIL;

	int state;
	if (json_get_val_int(&jobj, "power", &state) != WM_SUCCESS)
		return -1;

	plug_state = state;

	wmprintf("Led state set to %d", plug_state);

	return httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_SUCCESS,
				   strlen(HTTPD_JSON_SUCCESS),
				   HTTP_CONTENT_JSON_STR);
}

/* Define the JSON handler */
struct httpd_wsgi_call plug_wsgi_handlers[] = {
	{"/plug", HTTPD_DEFAULT_HDR_FLAGS | HTTPD_HDR_ADD_PRAGMA_NO_CACHE,
	 0, plug_http_get_power_state, plug_http_set_power_state, NULL, NULL},
};

static int plug_wsgi_handlers_no =
	sizeof(plug_wsgi_handlers) / sizeof(struct httpd_wsgi_call);

/*
 * Register Web-Service handlers
 *
 */
int register_httpd_plug_handlers()
{
	return httpd_register_wsgi_handlers(plug_wsgi_handlers,
					    plug_wsgi_handlers_no);
}
