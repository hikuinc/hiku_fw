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
#include <mdev_aes.h>
#include <json_parser.h>
#include <httpc.h>
#include <wmstdio.h>
#include <appln_dbg.h>
#define MAX_DOWNLOAD_DATA 1024
#define THREAD_COUNT 1
#define STACK_SIZE 8000

static const char *url = "https://www.wikipedia.org";
static SSL_CTX *wikipedia_ctx;
static os_thread_stack_define(test_thread_stack, STACK_SIZE);
static os_thread_t test_thread_handle[THREAD_COUNT];
static httpc_cfg_t httpc_cfg;
static tls_client_t cfg;
static http_session_t handle;
static http_resp_t *http_resp;

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

/* Establish secure HTTP connection */
static void httpc_secure_connect()
{
	int rv = httpc_get(url, &handle, &http_resp, &httpc_cfg);
	if (rv != WM_SUCCESS) {
		wmprintf("%s httpc_get: failed\r\n", url);
		return;
	}
	wmprintf("%s (%x): Status code: %d\r\n", url,
		 os_get_current_task_handle(),
		 http_resp->status_code);
}

/* Print data from webpage */
void print_data(http_session_t l_handle, const char *l_url)
{
	char *buf = os_mem_alloc(MAX_DOWNLOAD_DATA);
	if (!buf) {
		wmprintf("%s buf malloc failed\r\n", l_url);
		return;
	}
	int i, dsize, total_dsize = 0;
	while (1) {
		dsize = http_read_content(l_handle, buf, MAX_DOWNLOAD_DATA);
		if (dsize < 0) {
			wmprintf("%s (%x): Request %d bytes ...\r\n",
				 l_url, MAX_DOWNLOAD_DATA);

			wmprintf("%s : Read data failed: %d", l_url, dsize);
			break;
		}
		if (dsize == 0) {
			wmprintf("\r\n%s: ** All data read **\r\n", l_url);
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

		total_dsize += dsize;

		/* Random length sleep */
		os_thread_sleep(os_msec_to_ticks(rand() % 10));
	}
	os_mem_free(buf);
	wmprintf("%s Read total %d bytes success\r\n", l_url, total_dsize);
	http_close_session(&l_handle);
}

/* HTTP secure get function */
static void httpc_secure_get(void *arg)
{
	int index = (int)arg;
	httpc_certificate();
	httpc_secure_connect();
	print_data(handle, url);
	test_thread_handle[index] = NULL;
	os_thread_delete(NULL);
}

/* cli function */
static void cmd_httpc_secure_get(int argc, char *argv[])
{
	char name_buf[20];
	int index = 0;
	snprintf(name_buf, sizeof(name_buf), "Thread-%d", index);
	os_thread_create(&test_thread_handle[index],
				 name_buf, httpc_secure_get,
				 (void *) index,
				 &test_thread_stack, OS_PRIO_3);
}

/* httpc secure get cli */
static struct cli_command httpc_secure_get_cmds[] = {
	{"httpc-secure-get", NULL, cmd_httpc_secure_get}
};

/* cli_init function (registration of cli)*/
int httpc_secure_get_cli_init(void)
{
		if (cli_register_commands(&httpc_secure_get_cmds[0],
						sizeof(httpc_secure_get_cmds) /
						sizeof(struct cli_command))) {
				wmprintf("Unable to register commands\r\n");
		}
	return 0;
}
