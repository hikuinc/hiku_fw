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
	dbg("Critical Error %d: %s\r\n", crit_errno,
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

uint8_t trpc_2g_chans[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
uint8_t trpc_2g_cfg_data[] = {0, 18, 1, 18, 2, 16, 3, 14, 4, 18, 5, 16, 6,
			14, 7, 18, 8, 16, 9, 14};

void fill_chan_trpc_2g_cfg_data(uint8_t num_chans, uint8_t num_mod_grps,
				chan_trpc_t *chan_trpc)
{
	int i;

	chan_trpc->num_chans = num_chans;

	for (i = 0; i < chan_trpc->num_chans; i++) {
		chan_trpc->chan_trpc_config[i].num_mod_grps = num_mod_grps;
		chan_trpc->chan_trpc_config[i].chan_desc.start_freq = 2407;
		chan_trpc->chan_trpc_config[i].chan_desc.chan_width = 20;
		chan_trpc->chan_trpc_config[i].chan_desc.chan_num =
						trpc_2g_chans[i];

		memcpy(chan_trpc->chan_trpc_config[i].chan_trpc_entry,
				trpc_2g_cfg_data,
				chan_trpc->chan_trpc_config->num_mod_grps *
				sizeof(wifi_chan_trpc_entry_t));
	}
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

	chan_trpc_t *chan_trpc = os_mem_alloc(sizeof(chan_trpc_t));

	if (chan_trpc) {
		memset(chan_trpc, 0x00, sizeof(chan_trpc_t));
		fill_chan_trpc_2g_cfg_data(14, 10, chan_trpc);
		dbg("TRPC set status %d\r\n", wlan_set_trpc(chan_trpc));
		os_mem_free(chan_trpc);
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
