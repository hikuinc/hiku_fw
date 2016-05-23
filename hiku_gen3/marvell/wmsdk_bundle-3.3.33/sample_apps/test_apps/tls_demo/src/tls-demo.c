/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
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
#include "ntpc.h"

#include <cli.h>
#include <cli_utils.h>

#include <cyassl/ctaocrypt/settings.h>

#include <appln_dbg.h>
#include <wlan.h>
#include <wm-tls.h>
#include "tls-demo.h"
#include <mdev_aes.h>
#include <app_framework.h>

enum {
	TLS_HTTPC_CLIENT,
	TLS_CERT_STANDARD_TEST,
	TLS_PARALLEL_CONNECTION_TEST
};

const char wikipedia_cert_filename[] = "wikipedia-cert.pem";
const char marvell_cert_filename[] = "marvell-cert.pem";

/*
 * In this test case we do not run our code in the cli thread context. This
 * is because tls library requires more stack that what the cli thread
 * has. So we have our own thread.
 */

/* Buffer to be used as stack */
static os_thread_stack_define(tls_demo_stack, 8192);
static char url[256];

#define MAX_DOWNLOAD_DATA 1024

#define TLS_PARALLEL_CONNECTION_COUNT 3

const char *wikipedia_url = "https://www.wikipedia.org",
	*marvell_url = "https://www.marvell.com";
SSL_CTX *wikipedia_ctx, *marvell_ctx;

static int active_threads_remaining;
static int thread_report_fail;

struct fs *get_ftfs_handle(void);

static void tls_connect_and_download(os_thread_arg_t arg)
{
	int save;
	int select_url = (int) arg;
	const char *url;

	httpc_cfg_t httpc_cfg;
	memset(&httpc_cfg, 0x00, sizeof(httpc_cfg_t));

	switch (select_url) {
	case 0:
		url = wikipedia_url;
		httpc_cfg.ctx = wikipedia_ctx;
		break;
	case 1:
		url = marvell_url;
		httpc_cfg.ctx = marvell_ctx;
		break;
	default:
		thread_report_fail = true;
		wmprintf("Unknown URL\r\n");
		goto exit_thread;
	}

	http_session_t handle;
	http_resp_t *http_resp;
	wmprintf("%s (%x): Open\r\n", url, os_get_current_task_handle());
	int rv = httpc_get(url, &handle, &http_resp, &httpc_cfg);
	if (rv != WM_SUCCESS) {
		wmprintf("%s (%x): httpc_get: failed\r\n", url,
			 os_get_current_task_handle());
		thread_report_fail = true;
		goto exit_thread;
	}

	wmprintf("%s (%x): Status code: %d\r\n", url,
		 os_get_current_task_handle(),
		 http_resp->status_code);

	void *buf = os_mem_alloc(MAX_DOWNLOAD_DATA);
	if (!buf) {
		wmprintf("%s (%x): buf malloc failed\r\n", url,
			 os_get_current_task_handle());
		thread_report_fail = true;
		goto exit_thread;
	}

	int dsize, cum_dsize = 0;
	while (1) {
		dsize = http_read_content(handle, buf, MAX_DOWNLOAD_DATA);
		if (dsize < 0) {
			wmprintf("%s (%x): Request %d bytes ...\r\n",
				 url, os_get_current_task_handle(),
				 MAX_DOWNLOAD_DATA);

			wmprintf("%s (%x): Read data failed: %d",
				 url, os_get_current_task_handle(), dsize);
			thread_report_fail = true;
			break;
		}

		if (dsize == 0) {
			wmprintf("%s (%x): ******* All data read ******\r\n",
				 url, os_get_current_task_handle());
			break;
		}

		cum_dsize += dsize;

		/* Random length sleep */
		os_thread_sleep(os_msec_to_ticks(rand() % 10));
	}

	os_mem_free(buf);
	wmprintf("%s (%x): Read total %d bytes success\r\n",
		 url, os_get_current_task_handle(), cum_dsize);


	http_close_session(&handle);

exit_thread:
	save = os_enter_critical_section();
	active_threads_remaining--;
	os_exit_critical_section(save);

	os_thread_delete(NULL);
}

static void tls_parallel_connection_test()
{
	wmprintf("Synchronizing time with NTP\r\n");
	int rv = ntpc_sync("asia.pool.ntp.org", 0);
	if (rv != WM_SUCCESS)
		wmprintf("%s: NTP update failed\r\n", __func__);
	/*
	 * We as a client want to verify server certificate. Server does
	 * not need to verify ours so pass NULL
	 */
	app_tls_client_ftfs_t cfg;
	memset(&cfg, 0x00, sizeof(tls_client_t));

	cfg.ca_cert_filename = wikipedia_cert_filename;
	wikipedia_ctx = app_tls_create_client_context_ftfs(get_ftfs_handle(),
							   &cfg);
	if (!wikipedia_ctx) {
		wmprintf("%s: failed to create client context\r\n",
			 __func__);
		return;
	}

	memset(&cfg, 0x00, sizeof(tls_client_t));
	cfg.ca_cert_filename = marvell_cert_filename;
	marvell_ctx = app_tls_create_client_context_ftfs(get_ftfs_handle(),
							 &cfg);
	if (!marvell_ctx) {
		tls_purge_client_context(wikipedia_ctx);
		wmprintf("%s: failed to create client context\r\n",
			 __func__);
		return;
	}

	int cnt;

	active_threads_remaining = TLS_PARALLEL_CONNECTION_COUNT;
	/* Connect and send requests to all opened sockets */
	for (cnt = 0; cnt < TLS_PARALLEL_CONNECTION_COUNT; cnt++) {
		os_thread_create(NULL, /* thread handle */
				 "tls-dload", /* thread name */
				 tls_connect_and_download, /* entry function */
				 (void *)(cnt % 2), /* argument */
				 &tls_demo_stack, /* stack */
				 OS_PRIO_3);
	}

	/* Wait for child threads to finish execution */
	int timeout_secs = 60;
	while (active_threads_remaining && timeout_secs--)
		os_thread_sleep(os_msec_to_ticks(1000));

	if (active_threads_remaining)
		wmprintf("FAIL: Active threads still present\r\n");
	if (thread_report_fail)
		wmprintf("FAIL: Thread(s) report failure\r\n");
	if (!active_threads_remaining && !thread_report_fail)
		wmprintf("%s: Success\r\n", __func__);

	tls_purge_client_context(wikipedia_ctx);
	tls_purge_client_context(marvell_ctx);

	thread_report_fail = false;
}

static void tls_cert_test()
{
	wmprintf("Synchronizing time with NTP\r\n");
	ntpc_sync("asia.pool.ntp.org", 0);
	/*
	 * We as a client want to verify server certificate. Server does
	 * not need to verify ours so pass NULL
	 */
	app_tls_client_ftfs_t cfg;
	memset(&cfg, 0x00, sizeof(app_tls_client_ftfs_t));
	cfg.ca_cert_filename = "wikipedia-cert.pem";

	SSL_CTX *ctx = app_tls_create_client_context_ftfs(get_ftfs_handle(),
						      &cfg);
	if (!ctx) {
		wmprintf("%s: failed to create client context\r\n",
			 __func__);
		return;
	}

	http_session_t handle;
	http_resp_t *http_resp;
	httpc_cfg_t httpc_cfg;
	memset(&httpc_cfg, 0x00, sizeof(httpc_cfg_t));
	httpc_cfg.ctx = ctx;
	int rv = httpc_get("https://www.wikipedia.org", &handle,
			   &http_resp, &httpc_cfg);
	if (rv != WM_SUCCESS) {
		tls_purge_client_context(ctx);
		wmprintf("%s: httpc_get: failed\r\n", __func__);
		return;
	}

	wmprintf("Status code: %d\r\n", http_resp->status_code);

	int dsize;
	char buf[MAX_DOWNLOAD_DATA];
	while (1) {
		/* Keep reading data over the HTTPS session and print it out on
		 * the console. */
		dsize = http_read_content(handle, buf, MAX_DOWNLOAD_DATA);
		if (dsize < 0) {
			dbg("Unable to read data on http session: %d", dsize);
			break;
		}

		if (dsize == 0) {
			TLSD("********* All data read **********");
			break;
		}

		int i;
		for (i = 0; i < dsize; i++) {
			if (buf[i] == '\r')
				continue;
			if (buf[i] == '\n') {
				wmprintf("\n\r");
				continue;
			}
			wmprintf("%c", buf[i]);
		}
	}

	http_close_session(&handle);
	tls_purge_client_context(ctx);
}

static void tls_demo_main(os_thread_arg_t data)
{
	int test = (int) data;
	/*
	 * We will create a new thread here to process the ssl test case
	 * functionality. This is because the cli thread has a limited
	 * stack and ssl test cases need stack size of atleast
	 * TLS_DEMO_THREAD_STACK_SIZE to work properly.
	 */
	wmprintf("Heap before: %d\r\n", os_get_free_size());
	switch (test) {
	case TLS_HTTPC_CLIENT:
		tls_httpc_client(url);
		break;
	case TLS_CERT_STANDARD_TEST:
		tls_cert_test();
		break;
	case TLS_PARALLEL_CONNECTION_TEST:
		tls_parallel_connection_test();
		break;
	default:
		wmprintf("%s: Unknown test case\r\n", __func__);
		break;
	}

	wmprintf("Heap after: %d\r\n", os_get_free_size());
	/* Delete myself */
	os_thread_delete(NULL);
}

static void invoke_new_thread(void *arg)
{
	/*
	 * tls requires about 8K of stack. So, we cannot run tls API's in
	 * the cli thread context. Create a separate thread.
	 */
	os_thread_create(NULL,	/* thread handle */
			 "tls_demo",	/* thread name */
			 tls_demo_main,	/* entry function */
			 arg,	/* argument */
			 &tls_demo_stack,	/* stack */
			 OS_PRIO_2);	/* priority - medium low */
}

/* Invoked in cli thread context. */
static void cmd_tls_httpc_client(int argc, char **argv)
{
	if (argc != 2) {
		wmprintf("\nUsage: tls-httpc-client <url>\n\n\r");
		return;
	}

	strncpy(url, argv[1], sizeof(url));
	invoke_new_thread((void *)TLS_HTTPC_CLIENT);
}

static void cmd_tls_cert_test(int argc, char **argv)
{
	invoke_new_thread((void *)TLS_CERT_STANDARD_TEST);
}

static void cmd_tls_parallel_connection_test(int argc, char **argv)
{
	invoke_new_thread((void *)TLS_PARALLEL_CONNECTION_TEST);
}

static void cmd_ntp_time_update(int argc, char **argv)
{
	ntpc_sync("asia.pool.ntp.org", 0);
}

static struct cli_command tls_test_cmds[] = {
	{ "tls-http-client", "<url>", cmd_tls_httpc_client},
	{ "tls-cert-test", "", cmd_tls_cert_test},
	{ "tls-parallel-test", "", cmd_tls_parallel_connection_test},
	{ "ntp_sync", "Sync time with NTP server", cmd_ntp_time_update},
};

int tls_demo_init(void)
{
	int ret;

	/* Register all the commands */
	if (cli_register_commands(&tls_test_cmds[0],
		 sizeof(tls_test_cmds) / sizeof(struct cli_command))) {
		dbg("Unable to register CLI commands");
		while (1)
			;
		/* do nothing -- stall */
	}

	ret = aes_drv_init();
	if (ret != WM_SUCCESS) {
		while (1)
			;
		/* do nothing -- stall */
	}

	ret = tls_lib_init();
	if (ret != WM_SUCCESS) {
		while (1)
			;
		/* do nothing -- stall */
	}

	dbg("tls_demo application Started");

	return WM_SUCCESS;
}
