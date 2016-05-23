/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <httpc.h>
#include <httpd.h>
#include <websockets.h>
#include <lwip/api.h>

#define STACK_SIZE (1024*5)

static os_thread_stack_define(test_thread_stack, STACK_SIZE);
static os_thread_t test_thread_handle;

ws_context_t ws;
http_session_t hS;
char *url = "ws://echo.websocket.org";
const char *send_packet = "Welcome to Marvell 2015";

/*
 * This function makes a connection with the ws://echo.websocket.org
 */
static int create_websocket()
{
	int status;
	http_req_t req;
	httpc_cfg_t httpc_cfg;
	memset(&httpc_cfg, 0x00, sizeof(httpc_cfg_t));
	httpc_cfg.flags = 0;
	httpc_cfg.retry_cnt = DEFAULT_RETRY_COUNT;
	status = http_open_session(&hS, url, &httpc_cfg);
	if (status != WM_SUCCESS) {
		return -WM_FAIL;
	}
	wmprintf("HTTP connection established to %s\r\n", url);
	req.type = HTTP_GET;
	req.resource = "/";
	req.version = HTTP_VER_1_1;
	status = http_prepare_req(hS, &req, STANDARD_HDR_FLAGS);
	if (status != WM_SUCCESS) {
		http_close_session(&hS);
		return -WM_FAIL;
	}
	status = ws_upgrade_socket(&hS, &req, "chat", &ws);
	if (status != WM_SUCCESS) {
		http_close_session(&hS);
		return -WM_FAIL;
	}
	wmprintf("Socket upgraded to websocket\r\n");
	return WM_SUCCESS;
}

/*
 * This function sends data to the server
 */
static int send_data(void *arg)
{
	int ret;
	ws_frame_t f;
	char *data = (char *) arg;
	f.fin = 1;
	f.opcode = WS_TEXT_FRAME;
	f.data = (uint8_t *)data;
	f.data_len = strlen(data);
	wmprintf("Sending data: %s\r\n", data);
	ret = ws_frame_send(&ws, &f);
	if (ret <= 0) {
		return -WM_FAIL;
	} else
		return WM_SUCCESS;
}

/*
 * This function reads data from the server
 */
static int receive_data()
{
	int ret = 0;
	char buf[100];
	memset(buf, '\0', sizeof(buf));
	ws_frame_t f;
	ret = ws_frame_recv(&ws, &f);
	if (ret > 0) {
		strncpy(buf, (char *)f.data, ret);
		wmprintf("Received data: %s\r\n", buf);
		http_close_session(&hS);
		wmprintf("Connection closed\r\n");
		return WM_SUCCESS;
	} else {
		wmprintf("Error in read\r\n");
		http_close_session(&hS);
		ws_close_ctx(&ws);
		wmprintf("Connection closed\r\n");
		return -WM_FAIL;
	}
}

/* It calls the send and receive data */
static void websocket_test(void *arg)
{
	if (create_websocket() != WM_SUCCESS) {
		wmprintf("Error in connecting to cloud\r\n");
		return;
	}
	if (send_data(arg) == WM_SUCCESS)
		receive_data();
	else
		wmprintf("Error in sending data\r\n");
	test_thread_handle = NULL;
	os_thread_delete(NULL);
}

/*
 * Cli handle for websocket-test cli
 */
void cmd_websocket_handle(int argc, char **argv)
{
	if (argc < 2) {
		os_thread_create(&test_thread_handle,
			"Websocket-Test", websocket_test, (void *) send_packet,
			&test_thread_stack, OS_PRIO_3);
	} else {
		os_thread_create(&test_thread_handle,
			"Websocket-Test", websocket_test, (void *) argv[1],
			&test_thread_stack, OS_PRIO_3);
	}
}
