/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * HTTP Client demo application
 *
 * Summary:
 *
 * This application demonstartes HTTP client with
 * secure and non-secure connection.
 *
 * Description:
 *
 * It reads content of wikipedia webpage (http://wikipedia.org/)
 * and also posts a sample string to wikipedia.
 *
 * Note: The sample data that is posted to wikipedia is not visible to user.
 *
 * The CLIs are as follows:
 * 1: httpc-get (connects to http://api.evrythng.com/time)
 * 2: httpc-post (connects to http://posttestserver.com/post.php)
 * 3: httpc-secure-get (connects to https://wikipedia.org)
 * 4: httpc-secure-post (connects to https://wikipedia.org)
 *
 * Expected output:
 * 1: This prints the content of wikipedia webpage with status code 200 (OK)
 * 2: This post a sample data and returns status code 200 (OK)
 * 3: This prints the content of wikipedia webpage with status code 200 (OK)
 * 4: This post a sample data and returns status code 200 (OK)
 *
 */

#include <wm_os.h>
#include <app_framework.h>
#include <httpc.h>
#include <wmtime.h>
#include <cli.h>
#include <wmstdio.h>
#include <board.h>
#include <wmtime.h>
#include <psm.h>
#include <json_parser.h>
#include <critical_error.h>

extern void set_device_time();
extern int httpc_get_cli_init();
extern int httpc_secure_get_cli_init();
extern int httpc_secure_post_cli_init();
extern int httpc_post_cli_init();
extern int httpc_post_cb_cli_init();

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
		set_device_time();
		wmprintf("Normal Connected\r\n");
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
	/* Initialize time subsystem.
	 *
	 * Initializes time to 1/1/1970 epoch 0.
	 */
	ret = wmtime_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: wmtime_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	ret = httpc_get_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: httpc_get_cli_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	ret = httpc_secure_get_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: httpc_secure_get_cli_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	ret = httpc_post_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: httpc_post_cli_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	ret = httpc_post_cb_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: httpc_post_cb_cli_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	ret = httpc_secure_post_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: httpc_secure_post_cli_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	return;
}


int main()
{
	modules_init();

	wmprintf("Build Time: " __DATE__ " " __TIME__ "\r\n");

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		wmprintf("Failed to start application framework\r\n");
		 critical_error(-CRIT_ERR_APP, NULL);
	}
	return 0;
}
