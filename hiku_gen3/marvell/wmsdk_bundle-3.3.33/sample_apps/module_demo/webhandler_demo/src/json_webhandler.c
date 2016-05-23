/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */


#include <json_parser.h>
#include <json_generator.h>
#include <led_indicator.h>
#include <board.h>
#include <wm_os.h>
#include <httpd.h>
#include <appln_cb.h>
#include <appln_dbg.h>

/* Maximum Number of Tokens
 */
#define NUM_TOKENS	6
/* Maximum length of json string
 */
#define MAX_LENGTH_OF_JSON 40

static struct json_str jstr;
/* Function to blink LED
 */
static void demo_led_blink()
{
	led_blink(board_led_1(), 50, 50);
	os_thread_sleep(os_msec_to_ticks(1000));
	led_off(board_led_1());
}
/* LED state get handler
 */
static int demo_http_get_led_state(httpd_request_t *req)
{
	char content[30];
	json_str_init(&jstr, content, sizeof(content));
	json_start_object(&jstr);
	json_set_val_int(&jstr, "state", led_get_state(board_led_2()) ? 1 : 0);
	json_close_object(&jstr);
	return httpd_send_response(req, HTTP_RES_200, content,
		strlen(content), HTTP_CONTENT_JSON_STR);
}
/* LED state post handler
 */
static int demo_http_set_led_state(httpd_request_t *req)
{
	char led_state_change_req[MAX_LENGTH_OF_JSON];
	int ret;
	demo_led_blink();
	ret = httpd_get_data(req, led_state_change_req,
			sizeof(led_state_change_req));
	if (ret < 0) {
		dbg("Failed to get post request data");
		return ret;
	}
	/* Parse the JSON data */
	jsontok_t json_tokens[NUM_TOKENS];
	jobj_t jobj;
	ret = json_init(&jobj, json_tokens, NUM_TOKENS, led_state_change_req,
			req->body_nbytes);
	if (ret != WM_SUCCESS)
		return -WM_FAIL;

	int state;
	if (json_get_val_int(&jobj, "state",
			     &state) != WM_SUCCESS)
		return -1;

	if (state)
		led_on(board_led_2());
	else
		led_off(board_led_2());

	dbg("Led state set to %d", led_state);

	return httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_SUCCESS,
				   strlen(HTTPD_JSON_SUCCESS),
				   HTTP_CONTENT_JSON_STR);
}

struct httpd_wsgi_call json_wsgi_handlers[] = {
		{"/led-state", HTTPD_DEFAULT_HDR_FLAGS |
				HTTPD_HDR_ADD_PRAGMA_NO_CACHE,
				0, demo_http_get_led_state,
				demo_http_set_led_state, NULL, NULL}
};
/*
 * Register Web-Service handlers
 *
 */

static int json_wsgi_handlers_no =
	sizeof(json_wsgi_handlers) / sizeof(struct httpd_wsgi_call);

int register_httpd_json_handlers()
{
	return httpd_register_wsgi_handlers(json_wsgi_handlers,
		json_wsgi_handlers_no);
}
