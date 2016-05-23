/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <openssl/ssl.h>
#include <appln_dbg.h>
#include <wmtypes.h>
#include <wmstdio.h>
#include <wmtime.h>
#include <string.h>
#include <wmstdio.h>
#include <wm_net.h>
#include <httpc.h>
#include <cli.h>
#include <cli_utils.h>
#include <cyassl/ctaocrypt/settings.h>
#include <wlan.h>
#include <wm-tls.h>
#include <mdev_aes.h>
#include <json_parser.h>
#include <httpc.h>
#include <wmstdio.h>
#define MAX_DOWNLOAD_DATA 80
#define MAX_URL_LEN 128
#define THREAD_COUNT 1
#define STACK_SIZE 8000

static const char *url = "https://www.wikipedia.org";
static SSL_CTX *wikipedia_ctx;
static os_thread_stack_define(test_thread_stack, STACK_SIZE);
static os_thread_t test_thread_handle[THREAD_COUNT];
static httpc_cfg_t httpc_cfg;
static tls_client_t cfg;
static http_session_t handle;
static const char *data = "Welcome to Marvell India 2015";
static http_resp_t *resp;

extern const unsigned char wikipedia_server_cert[];
extern const int wiki_cert_size;

/* Create certificate */
static void httpc_certificate()
{
	wmprintf("Connecting to : %s \r\n", url);
	cfg.ca_cert = wikipedia_server_cert;
	cfg.ca_cert_size = wiki_cert_size - 1;
	cfg.client_cert = NULL;
	cfg.client_cert_size = 0;
	wikipedia_ctx = tls_create_client_context(&cfg);
	if (!wikipedia_ctx) {
		wmprintf("%s: failed to create client context\r\n",
				__func__);
	return;
	}
	httpc_cfg.ctx = wikipedia_ctx;
}

/* Establish secure HTTP connection and post data */
static void httpc_secure_connect_post()
{

	dbg("Website: %s\r\n", url);
	dbg("Data to post: %s\r\n", data);
	int len = strlen(data);
	int rv = http_open_session(&handle, url, &httpc_cfg);
	if (rv != 0) {
		dbg("Open session failed: %s (%d)", url, rv);
		return;
	}
	http_req_t req = {
		.type = HTTP_POST,
		.resource = url,
		.version = HTTP_VER_1_1,
		.content = data,
		.content_len = len,
	};

	rv = http_prepare_req(handle, &req,
				  STANDARD_HDR_FLAGS |
				  HDR_ADD_CONN_KEEP_ALIVE);
	if (rv != 0) {
		dbg("Prepare request failed: %d", rv);
		return;
	}

	rv = http_send_request(handle, &req);
	if (rv != 0) {
		dbg("Send request failed: %d", rv);
		return;
	}
	rv = http_get_response_hdr(handle, &resp);
	wmprintf("%s (%x): Status code: %d\r\n", url,
	 os_get_current_task_handle(),
	 resp->status_code);

	if (rv != 0) {
		dbg("Get resp header failed: %d", rv);
		return;
	}
}

/* HTTP secure post function */
static void httpc_secure_post(void *arg)
{
	int index = (int)arg;
	httpc_certificate();
	httpc_secure_connect_post();
	test_thread_handle[index] = NULL;
	os_thread_delete(NULL);
}

/* cli function */
static void cmd_httpc_secure_post(int argc, char *argv[])
{
	char name_buf[20];
	int index = 0;
	snprintf(name_buf, sizeof(name_buf), "Thread-%d", index);
	os_thread_create(&test_thread_handle[index],
				 name_buf, httpc_secure_post,
				 (void *) index,
				 &test_thread_stack, OS_PRIO_3);
}

/* httpc secure post cli */
static struct cli_command httpc_secure_post_cmds[] = {
	{"httpc-secure-post", NULL, cmd_httpc_secure_post}
};

/* cli_init function (registration of cli)*/
int httpc_secure_post_cli_init(void)
{
		if (cli_register_commands(&httpc_secure_post_cmds[0],
						sizeof(httpc_secure_post_cmds) /
						sizeof(struct cli_command))) {
				wmprintf("Unable to register commands\r\n");
		}
	return 0;
}
