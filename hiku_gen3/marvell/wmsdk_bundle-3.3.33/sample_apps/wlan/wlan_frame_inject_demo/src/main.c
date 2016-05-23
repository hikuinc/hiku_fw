/*
 *  Copyright (C) 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Simple WLAN Application using wlan frame inject API
 *
 */
#include <wm_os.h>
#include <app_framework.h>
#include <wmtime.h>
#include <partition.h>
#include <appln_cb.h>
#include <appln_dbg.h>
#include <cli.h>
#include <psm.h>
#include <wmstdio.h>
#include <wmsysinfo.h>
#include <wm_net.h>
#include <critical_error.h>

/*-----------------------Global declarations----------------------*/

/* This function is defined for handling critical error.
 * For this application, we just stall and do nothing when
 * a critical error occurs.
 */
void critical_error(int crit_errno, void *data)
{
	wmprintf("Critical Error %d: %s\r\n", crit_errno,
			critical_error_msg(crit_errno));
	while (1)
		;
	/* do nothing -- stall */
}

/*
 * A simple HTTP Web-Service Handler
 *
 * Returns the string "Hello World" when a GET on http://<IP>/hello
 * is done.
 */

char *hello_world_string = "Hello World\n";

int hello_handler(httpd_request_t *req)
{
	char *content = hello_world_string;

	httpd_send_response(req, HTTP_RES_200,
			    content, strlen(content),
			    HTTP_CONTENT_PLAIN_TEXT_STR);
	return WM_SUCCESS;
}

struct httpd_wsgi_call hello_wsgi_handler = {
	"/hello",
	HTTPD_DEFAULT_HDR_FLAGS,
	0,
	hello_handler,
	NULL,
	NULL,
	NULL
};

/*
 * Register Web-Service handlers
 *
 */
int register_httpd_handlers()
{
	return httpd_register_wsgi_handlers(&hello_wsgi_handler, 1);
}

/*
 * Handler invoked when the Micro-AP Network interface
 * is ready.
 *
 */

#define BUFFER_LENGTH       (2 * 1024)

static void inject_frame()
{
	/** Modify below values to inject different type and subtypes of
	 * frames */
	uint16_t type = 0x00, subtype = 0x05;
	uint16_t seq_num = 0, frag_num = 0, from_ds = 0, to_ds = 0;
	uint8_t dest[] = {0x10, 0x6F, 0x3F, 0x4D, 0x84, 0x44};
	uint8_t bssid[] = {0x10, 0x6F, 0x3F, 0x4D, 0x84, 0x44};
	uint8_t data[] = {0x01, 0x01, 0x00, 0x0c, 0x00, 0x58, 0x02, 0x40};

	/*
		type=0x00
			# MGMT frames	       0
			# CONTROL frames       1
			# DATA frames          2


		subtype=0x05
		    for MGMT frames(type: 0x00)
			# Assoc Request        0
			# Assoc Response       1
			# Re-Assoc Request     2
			# Re-Assoc Response    3
			# Probe Request        4
			# Probe Response       5
			# Beacon               8
			# Atim                 9
			# Dis-assoc            10
			# Auth                 11
			# Deauth               12
			# Action Frame         13
		    for CONTROL frames(type: 0x01)
			# Block ACK	       10
			# RTS		       11
			# ACK		       13
		    for DATA frames(type: 0x02)
			# Data		       0
			# Null (no data)       4
			# QoS Data	       8
			# QoS Null (no data)   12

	 * seq_num means Sequence number
	 * frag_num means Fragmentation number
	 * to_ds = 1 means To the Distribution system
	 * from_ds = 1 means Exit from the Distribution system
	 * dest Destination MAC address
	 * bssid BSSID
	 * data Data in payload.
	 */

	/* Do not modify below code as it prepares WLAN 802.11
	 * frame from values specified above.
	 */
	uint16_t data_len = sizeof(wifi_mgmt_frame_tx);
	unsigned char mac[6] = { 0 };
	uint8_t *buffer = NULL;
	wifi_mgmt_frame_tx *pmgmt_frame = NULL;

	buffer = (uint8_t *)os_mem_alloc(BUFFER_LENGTH);
	if (!buffer) {
		wmprintf("ERR:Cannot allocate memory!\r\n");
		return;
	}

	memset(buffer, 0, BUFFER_LENGTH);

	wlan_get_mac_address(mac);

	pmgmt_frame = (wifi_mgmt_frame_tx *)(buffer);

	pmgmt_frame->frm_ctl = (type & 0x3) << 2;

	pmgmt_frame->frm_ctl |= subtype << 4;

	memcpy(pmgmt_frame->addr1, dest, MLAN_MAC_ADDR_LENGTH);
	memcpy(pmgmt_frame->addr2, mac, MLAN_MAC_ADDR_LENGTH);
	memcpy(pmgmt_frame->addr3, bssid, MLAN_MAC_ADDR_LENGTH);

	memcpy(pmgmt_frame->payload, data, sizeof(data));
	data_len += sizeof(data);

	pmgmt_frame->seq_ctl = seq_num << 4;
	pmgmt_frame->seq_ctl |= frag_num;
	pmgmt_frame->frm_ctl |= (from_ds & 0x1) << 9;
	pmgmt_frame->frm_ctl |= (to_ds & 0x1) << 8;

	pmgmt_frame->frm_len = data_len - sizeof(pmgmt_frame->frm_len);

	/* Probe Response Frames will get injected on default channel 6 */
	wlan_inject_frame(buffer, data_len);

	if (buffer)
		os_mem_free(buffer);
}

/*
 * Handler invoked when WLAN subsystem is ready.
 *
 * The app-framework tells the handler whether there is
 * valid network information stored in persistent memory.
 *
 * The handler can then chose to connect to the network.
 *
 * We ignore the data and just start a Micro-AP network
 * with DHCP service. This will allow a client device
 * to connect to us and receive a valid IP address via
 * DHCP.
 */
void event_wlan_init_done(void *data)
{
	int ret;
	/*
	 * Initialize CLI Commands for some of the modules:
	 *
	 * -- psm:  allows user to check data in psm partitions
	 * -- wlan: allows user to explore basic wlan functions
	 */

	ret = psm_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: psm_cli_init failed");
	ret = wlan_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_cli_init failed");

	dbg("Start Frame injection on default channel : 6");

	ret = 1;
	while (ret < 5) {
		inject_frame();
		os_thread_sleep(os_msec_to_ticks(5000));
		ret++;
	}

	ret = 1;

	dbg("Start Frame injection on channel : 1");
	wlan_remain_on_channel(true, 1, 50000);

	while (ret < 10) {
		inject_frame();
		os_thread_sleep(os_msec_to_ticks(5000));
		ret++;
		if (ret == 3) {
			dbg("Stop Frame injection on channel 1 "
				"and go back to default channel 6");
			wlan_remain_on_channel(false, 1, 1000);
		}
	}

}

/* This is the main event handler for this project. The application framework
 * calls this function in response to the various events in the system.
 */
int common_event_handler(int event, void *data)
{
	switch (event) {
	case AF_EVT_WLAN_INIT_DONE:
		event_wlan_init_done(data);
		break;
	default:
		break;
	}

	return 0;
}

static void modules_init()
{
	int ret;

	/*
	 * Initialize wmstdio prints
	 */
	ret = wmstdio_init(UART0_ID, 0);
	if (ret != WM_SUCCESS) {
		dbg("Error: wmstdio_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	/*
	 * Initialize CLI Commands
	 */
	ret = cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: cli_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	/* Initialize time subsystem.
	 *
	 * Initializes time to 1/1/1970 epoch 0.
	 */
	ret = wmtime_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: wmtime_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	/*
	 * Register Power Management CLI Commands
	 */
	ret = pm_cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: pm_cli_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	/*
	 * Register Time CLI Commands
	 */
	ret = wmtime_cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: wmtime_cli_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	return;
}

int main()
{
	modules_init();

	dbg("Build Time: " __DATE__ " " __TIME__ "");

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		dbg("Failed to start application framework");
				critical_error(-CRIT_ERR_APP, NULL);
	}
	return 0;
}
