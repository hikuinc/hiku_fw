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
#include <appln_dbg.h>
#define MAX_DOWNLOAD_DATA 80
#define MAX_URL_LEN 128
#define STACK_SIZE 8000
#define THREAD_COUNT 1

static const char *url = "http://posttestserver.com/post.php";
static os_thread_stack_define(test_thread_stack, STACK_SIZE);
static os_thread_t test_thread_handle[THREAD_COUNT];
static http_session_t handle;
static const char *data = "Welcome to Marvell India 2015";
static http_resp_t *resp;

/* Print data from webpage */
static void print_data()
{
	char *buf = os_mem_alloc(MAX_DOWNLOAD_DATA);
	if (!buf) {
		wmprintf("%s buf malloc failed\r\n", url);
		return;
	}
	int i, dsize, cum_dsize = 0;
	while (1) {
		dsize = http_read_content(handle, buf, MAX_DOWNLOAD_DATA);
		if (dsize < 0) {
			wmprintf("%s (%x): Request %d bytes ...\r\n",
				 url, MAX_DOWNLOAD_DATA);

			wmprintf("%s : Read data failed: %d", url, dsize);
			break;
		}
		if (dsize == 0) {
			wmprintf("\r\n%s: ** All data read **\r\n", url);
			break;
		}

		for (i = 0; i < dsize; i++) {
				if (buf[i] == '\r') {
						continue;
				}
				if (buf[i] == '\n') {
						wmprintf("\r\n");
						continue;
				}
				wmprintf("%c", buf[i]);
		}

		cum_dsize += dsize;

		/* Random length sleep */
		os_thread_sleep(os_msec_to_ticks(rand() % 10));
	}
	os_mem_free(buf);
	wmprintf("%s Read total %d bytes success\r\n", url, cum_dsize);
	http_close_session(&handle);
}
/* Establish HTTP connection and post data */
static void httpc_connect_post()
{
	wmprintf("Connecting to : %s \r\n", url);
	dbg("Website: %s\r\n", url);
	dbg("Data to post: %s\r\n", data);
	int len = strlen(data);
	int rv = http_open_session(&handle, url, NULL);
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

/* HTTP post function */
static void httpc_post_url(void *arg)
{
	int index = (int)arg;
	httpc_connect_post();
	print_data();
	test_thread_handle[index] = NULL;
	os_thread_delete(NULL);
}

/* cli function */
static void cmd_httpc_post(int argc, char *argv[])
{
	char name_buf[20];
	int index = 0;
	snprintf(name_buf, sizeof(name_buf), "Thread-%d", index);
	os_thread_create(&test_thread_handle[index],
				 name_buf, httpc_post_url,
				 (void *) index,
				 &test_thread_stack, OS_PRIO_3);
}

/* httpc post cli */
static struct cli_command httpc_post_cmds[] = {
	{"httpc-post", NULL, cmd_httpc_post}
};

/* cli_init function (registration of cli)*/
int httpc_post_cli_init(void)
{
		if (cli_register_commands(&httpc_post_cmds[0],
						sizeof(httpc_post_cmds) /
						sizeof(struct cli_command))) {
				wmprintf("Unable to register commands\r\n");
		}
	return 0;
}
