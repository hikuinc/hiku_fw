/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <openssl/ssl.h>
#include <wmtypes.h>
#include <wmstdio.h>
#include <wmtime.h>
#include <string.h>
#include <wmstdio.h>
#include <wm_os.h>
#include <wm_net.h>
#include <httpc.h>
#include <cli.h>
#include <cli_utils.h>
#include <cyassl/ctaocrypt/settings.h>
#include <appln_dbg.h>
#include <wlan.h>
#include <wm-tls.h>
#include <appln_dbg.h>
#include <mdev_aes.h>
#include <json_parser.h>
#include <httpc.h>
#include <wmstdio.h>

#define MAX_DOWNLOAD_DATA 1024
#define STACK_SIZE 8000

static const char *url = "http://s0.cyberciti.org/images/misc/static/2012/11/" \
					"ifdata-welcome-0.png";
static os_thread_stack_define(test_thread_stack, STACK_SIZE);
static os_thread_t test_thread_handle;
static http_session_t handle;
static http_resp_t *http_resp;

extern void print_data(http_session_t l_handle, const char *l_url);

static int httpc_get_with_extra_hdr(char *hdr)
{
	int status;

	status = http_open_session(&handle, url, NULL);
	if (status != WM_SUCCESS) {
		wmprintf("Open session failed URL: %s\r\n", url);
		return -WM_FAIL;
	}

	http_req_t req;
	req.type = HTTP_GET;
	req.resource = url;
	req.version = HTTP_VER_1_1;

	status = http_prepare_req(handle, &req, STANDARD_HDR_FLAGS);
	if (status != WM_SUCCESS) {
		wmprintf("preparing request failed: %s\r\n", url);
		goto err;
	}

	char *key, *value;
	key = strtok(hdr, ":");
	value = strtok(NULL, ":");

	if (key == NULL || value == NULL) {
		wmprintf("Error parsing HTTP header: %s:%s\r\n", key, value);
		goto err;
	}

	status = http_add_header(handle, &req, key, value);
	if (status != WM_SUCCESS) {
		wmprintf("Error adding new header: %s:%s\r\n", key, value);
		goto err;
	}

	status = http_send_request(handle, &req);
	if (status != WM_SUCCESS) {
		wmprintf("Request send failed: URL: %s\r\n", url);
		goto err;
	}

	status = http_get_response_hdr(handle, &http_resp);
	if (status != WM_SUCCESS) {
		wmprintf("Failed to get response header: URL: %s\r\n", url);
		goto err;
	}

	return WM_SUCCESS;
err:
	http_close_session(&handle);
	return status;
}

/* Establish HTTP connection */
static int httpc_connect(char *arg)
{
	int rv;

	wmprintf("Connecting to : %s \r\n", url);

	if (arg) {
		rv = httpc_get_with_extra_hdr(arg);
		if (rv != WM_SUCCESS) {
			wmprintf("%s httpc_get: failed\r\n", url);
			return rv;
		}
	 } else {
		rv = httpc_get(url, &handle, &http_resp, NULL);
		if (rv != WM_SUCCESS) {
			wmprintf("%s httpc_get: failed\r\n", url);
			return rv;
		}
	}
	wmprintf("%s (%x): Status code: %d\r\n", url,
		 os_get_current_task_handle(),
		 http_resp->status_code);
	return WM_SUCCESS;
}

/* HTTP get function */
static void httpc_get_url(void *arg)
{
	int ret = httpc_connect((char *)arg);
	if (ret == WM_SUCCESS)
		print_data(handle, url);
	test_thread_handle = NULL;
	os_thread_delete(NULL);
}

/* cli function */
static void cmd_httpc_get(int argc, char *argv[])
{
	char *arg = NULL;

	if (argc == 2)
		arg = argv[1];

	os_thread_create(&test_thread_handle,
				 "httpc-thread", httpc_get_url,
				 (void *) arg,
				 &test_thread_stack, OS_PRIO_3);
}

/* httpc secure get cli */
static struct cli_command httpc_get_cmds[] = {
	{"httpc-get", NULL, cmd_httpc_get},
	{"httpc-get-with-extra-hdr", "<HTTP header>", cmd_httpc_get}
};

/* cli_init function (registration of cli)*/
int httpc_get_cli_init(void)
{
		if (cli_register_commands(&httpc_get_cmds[0],
						sizeof(httpc_get_cmds) /
						sizeof(struct cli_command))) {
				wmprintf("Unable to register commands\r\n");
		}
	return 0;
}
