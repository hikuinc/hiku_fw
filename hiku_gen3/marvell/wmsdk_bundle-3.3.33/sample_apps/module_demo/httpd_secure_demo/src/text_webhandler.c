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
/*
 * A simple HTTP Web-Service get Handler
 *
 * Returns the string "SSL Test Passed" when a GET on
 * https://192.169.10.1/ssl-test is done.
 */
int ssl_test_get_handler(httpd_request_t *req)
{
	char *content = "SSL Test Passed";
	httpd_send_response(req, HTTP_RES_200,
			    content, strlen(content),
			    HTTP_CONTENT_PLAIN_TEXT_STR);
	return WM_SUCCESS;
}


struct httpd_wsgi_call text_wsgi_handlers[] = {
	{"/ssl-test", HTTPD_DEFAULT_HDR_FLAGS | HTTPD_HDR_ADD_PRAGMA_NO_CACHE,
			0, ssl_test_get_handler, NULL, NULL, NULL}
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
