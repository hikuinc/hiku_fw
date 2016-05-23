/*
 * http_manager.c
 *
 *  Created on: May 16, 2016
 *      Author: rajan-work
 */

#include <httpd.h>
#include "http_manager.h"
#include "connection_manager.h"

/** forward declarations */
static int hiku_http_get_status(httpd_request_t *req);
static int hiku_http_set_status(httpd_request_t *req);

/** end of forward declarations */


/** global variable definitions */

#define NUM_TOKENS 6
static int led_status = 0;

//fs handle for the FFTS file system that contains the WWW folder for provisioning
static struct fs *fs_handle;

/* Define the JSON handler */
struct httpd_wsgi_call hiku_wsgi_handlers[] = { {"/hiku", HTTPD_DEFAULT_HDR_FLAGS |
                          HTTPD_HDR_ADD_PRAGMA_NO_CACHE,
                    0,
                   hiku_http_get_status,
                   hiku_http_set_status,
                   NULL, NULL},
};

static int hiku_wsgi_handlers_no = sizeof(hiku_wsgi_handlers)/sizeof(struct httpd_wsgi_call);



/** function implementations */

static int hiku_http_get_status(httpd_request_t *req)
{
	char content[100];
	snprintf(content, sizeof(content),"{ \"msg\":\"Welcome to gen3 hiku!!\"}");
	return httpd_send_response(req, HTTP_RES_200, content, strlen(content), HTTP_CONTENT_JSON_STR);
}

static int hiku_http_set_status(httpd_request_t *req)
{
	char plug_data[30];
	int ret = httpd_get_data(req, plug_data, sizeof(plug_data));
	if (ret < 0)
	{
		hiku_h("Failed to get post request data");
		return ret;
	}

	/* Parse the JSON data */
	jsontok_t json_tokens[NUM_TOKENS];
	jobj_t jobj;
	ret = json_init(&jobj, json_tokens, NUM_TOKENS,
	                  plug_data, req->body_nbytes);
	if (ret != WM_SUCCESS)
	    return -WM_FAIL;
	int state;
	if (json_get_val_int(&jobj, "power", &state) != WM_SUCCESS)
	    return -1;
	led_status = state;
	hiku_h("Led state set to %d", led_status);
	return httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_SUCCESS,
	                           strlen(HTTPD_JSON_SUCCESS),
	                           HTTP_CONTENT_JSON_STR);
}


int http_manager_init(void)
{
	// Initialize the HTTP web service on the device
	hiku_h("HTTP Manager is initializing!!\r\n");

	if (app_httpd_with_fs_start(FTFS_API_VERSION, FTFS_PART_NAME, &fs_handle) != WM_SUCCESS)
	{
		hiku_h("Error: Failed to start HTTPD\r\n");
		return WM_FAIL;
	}

	if (httpd_register_wsgi_handlers(hiku_wsgi_handlers, hiku_wsgi_handlers_no ) != WM_SUCCESS)
	{
		hiku_h("HTTP handler registration failed!!\r\n");
		return WM_FAIL;
	}
	return WM_SUCCESS;
}
