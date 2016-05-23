/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Websocket application
 *
 * Summary:
 *
 * This application demonstrates a websocket against a echo server
 * Description:
 * The application open a websocket connection to a server
 * Link to echo server ws://echo.websocket.org
 * The CLIs are as follows:
 * websocket-test
 * Output:
 * Sending data: Welcome to Marvell 2015
 * Received data: Welcome to Marvell 2015
 *
 * It makes a connection request to ws://echo.websocket.org
 * Once the connection is established, application sends a string message.
 * The Server echoes back the same string message.
 * Application receives the string and displays it back.
 */

#include <wm_os.h>
#include <app_framework.h>
#include <httpc.h>
#include <cli.h>
#include <wmstdio.h>
#include <board.h>
#include <psm.h>
#include <json_parser.h>
#include <critical_error.h>

void cmd_websocket_handle();

/*Common event handler*/
int common_event_handler(int event, void *data)
{
	int ret;
	switch (event) {
	case AF_EVT_WLAN_INIT_DONE:
		ret = psm_cli_init();
		if (ret != WM_SUCCESS)
			wmprintf("Error: psm_cli_init failed\r\n");
		int i = (int) data;
		if (i == APP_NETWORK_NOT_PROVISIONED) {
			wmprintf("\r\nPlease provision the device "
				 "and then reboot it:\r\n\r\n");
			wmprintf("psm-set network ssid <ssid>\r\n");
			wmprintf("psm-set network security <security_type>"
				 "\r\n");
			wmprintf("    where: security_type: 0 if open,"
				 " 3 if wpa, 4 if wpa2\r\n");
			wmprintf("psm-set network passphrase <passphrase>"
				 " [valid only for WPA and WPA2 security]\r\n");
			wmprintf("psm-set network configured 1\r\n");
			wmprintf("pm-reboot\r\n\r\n");
		} else
			app_sta_start();
		break;
	case AF_EVT_NORMAL_CONNECTED:
		wmprintf("Normal Connected\r\n");
		wmprintf("You can enter CLI: websocket-test [<data>]\r\n");
		break;
	default:
		break;
	}
	return 0;
}

/* App defined critical error */
enum app_crit_err {
	CRIT_ERR_APP = CRIT_ERR_LAST + 1,
};
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

static struct cli_command websocket_cmds[] = {
	{"websocket-test", NULL, cmd_websocket_handle}
};

/* websocket cli init function (registration of cli) */
int websocket_cli_init(void)
{
		if (cli_register_commands(&websocket_cmds[0],
						sizeof(websocket_cmds) /
						sizeof(struct cli_command))) {
				wmprintf("Unable to register commands\r\n");
		}
	return 0;
}

/* All required modules are initialised here */
static void modules_init()
{
	int ret;

	ret = wmstdio_init(UART0_ID, 0);
	if (ret != WM_SUCCESS) {
		wmprintf("Error: wmstdio_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	ret = cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: cli_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	ret = wlan_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: wlan_cli_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	ret = pm_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: pm_cli_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	ret = websocket_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: websocket_cli_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	return;
}


int main()
{
	modules_init();
	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		wmprintf("Failed to start application framework\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	return 0;
}
