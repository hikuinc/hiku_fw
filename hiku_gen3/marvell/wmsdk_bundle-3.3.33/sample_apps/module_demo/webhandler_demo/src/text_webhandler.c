/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */


#include <led_indicator.h>
#include <board.h>
#include <wm_os.h>
#include <httpd.h>
#include <appln_cb.h>
#include <appln_dbg.h>
/* Function to blink LED
 */
static void demo_led_blink()
{
	led_blink(board_led_1(), 50, 50);
	os_thread_sleep(os_msec_to_ticks(1000));
	led_off(board_led_1());
}

/*
 * A simple HTTP Web-Service get Handler
 *
 * Returns the string "Hello World" when a GET on http://<IP>/hello
 * is done.
 */
int hello_get_handler(httpd_request_t *req)
{
	char *content = "Hello World";
	httpd_send_response(req, HTTP_RES_200,
			    content, strlen(content),
			    HTTP_CONTENT_PLAIN_TEXT_STR);
	return WM_SUCCESS;
}

/*
 * A simple HTTP Web-Service post Handler
 * Return a input string concatenated with "Hello " when post on
 * http://<IP>/hello
 */

int hello_post_handler(httpd_request_t *req)
{
	char hello_text[30], msg[40];
	memset(hello_text, '\0', sizeof(hello_text));
	memset(msg, '\0', sizeof(msg));
	int ret;

	ret = httpd_get_data(req, hello_text,
			sizeof(hello_text));

	if (ret < 0) {
		dbg("Failed to get post request data");
		return ret;
	}
	int state = 0;

	if (hello_text[0] != '\0') {
			state = 1; /* Some text is entered*/
	}
	if (state)
		demo_led_blink();
	else
		led_off(board_led_1());

	strcat(msg, "Hello ");
	strcat(msg, hello_text);

	return httpd_send_response(req, HTTP_RES_200,
			    msg, strlen(msg),
			    HTTP_CONTENT_PLAIN_TEXT_STR);
}

struct httpd_wsgi_call text_wsgi_handlers[] = {
	{"/hello", HTTPD_DEFAULT_HDR_FLAGS | HTTPD_HDR_ADD_PRAGMA_NO_CACHE,
			0, hello_get_handler, hello_post_handler, NULL, NULL}
};

/*
 * Register Web-Service handlers
 *
 */

static int text_wsgi_handlers_no =
	sizeof(text_wsgi_handlers) / sizeof(struct httpd_wsgi_call);

int register_httpd_text_handlers()
{
	return httpd_register_wsgi_handlers(text_wsgi_handlers,
		text_wsgi_handlers_no);
}
