/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* Application demonstrating mid level (component level)
 * and high level (http level) firmware upgrade APIs
 *
 * Summary:
 *
 * This application showcases the use of mid level and high level
 * firmware upgrade APIs. By default, the mid level APIs are used.
 * To switch to the High level APIs, change the configuration option in
 * sample_apps/module_demo/fw_upgrade/fw_upgrade_demo/config.mk.
 * Clean the application and then rebuild.
 *
 * Mid Level (Component Level) APIs:
 * - Applications need to write own data fetch function
 * - Same APIs can be used for server/client mode by passing appropriate data
 * fetch function
 * - Different APIs for different components like MCU Firmware, WI-Fi firmware
 *   and FTFS
 *
 * High Level (HTTP level) APIs:
 * - No need to write any data fetch function. That is managed internally.
 * - Different APIs for different modes (serve/client)
 * - Same APIs can be used for different components like MCU Firmware,
 *   WI-Fi firmware and FTFS by passing appropriate partition pointer.
 *
 *
 * Description:
 *
 * In order to use firmware upgrades, the device first needs to be provisioned.
 * To keep the app simple, provisioning needs to be done using CLI. The
 * appropriate commands will be shown on the console on boot-up.
 *
 * When the device connects to a network, the IP address will be displayed
 * on the console. This can be then used to demonstrate firmware upgrade.
 *
 * Two upgrade APIs are exposed by this app.
 * 1. Client mode firmware upgrades
 * curl -d '{"url":"http://<server_ip>/link/to/fw.bin"}' \
 *	http://<device_ip>/fw_url
 * - The secure upgrade image is fetched from the remote server based on
 *   the url.
 *
 * 2. Server mode firmware upgrades
 * curl --data-binary @fw.bin http://<device_ip>/fw_data
 * - The secure upgrade image is POSTed directly to the device
 *
 * fw.bin should be the raw firmware binary.
 * On successful upgrade, {"success":0} will be returned and device will
 * reboot.
 * On failure, {"error":-1} will be returned.
 *
 */
#include <wm_os.h>
#include <app_framework.h>
#include <cli.h>
#include <wmstdio.h>
#include <httpd.h>
#include <led_indicator.h>
#include <board.h>
#include <psm.h>
#include <critical_error.h>
#include <rfget.h>
static void app_reboot_cb()
{
	ll_printf("Rebooting...");
	pm_reboot_soc();
}
/* Start a timer of 3 seconds and initiate reboot in the timer callback.
 * This gives the HTTP Server some time to send the response to the client.
 */
static int reboot_device()
{
	static os_timer_t app_reboot_timer;
	if (!app_reboot_timer) {
		if (os_timer_create(&app_reboot_timer, "app_reboot_timer",
					os_msec_to_ticks(3000),
					app_reboot_cb, NULL,
					OS_TIMER_ONE_SHOT,
					OS_TIMER_AUTO_ACTIVATE)
				!= WM_SUCCESS)
			return -WM_FAIL;

	}
	return WM_SUCCESS;
}
#ifdef APPCONFIG_MID_LEVEL_FW_UPGRADE
static size_t server_mode_data_fetch_func(void *priv, void *buf, size_t max_len)
{
	httpd_request_t *req = (httpd_request_t *)priv;
	int size_read = httpd_get_data(req, buf, max_len);
	if (size_read < 0) {
		wmprintf("Could not read data from stream\r\n");
		return -WM_FAIL;
	}
	return size_read;
}
static size_t client_mode_data_fetch_func(void *priv, void *buf, size_t max_len)
{
	http_session_t handle = (http_session_t) priv;
	return http_read_content(handle, buf, max_len);
}
static int server_mode_upgrade_fw(httpd_request_t *req, int filesize)
{
	struct partition_entry *p = rfget_get_passive_firmware();
	/* Upgrade the mcu firmware image. Validation function passed as NULL
	 * as we are not performing any particular app level validation.
	 * Length and CRC are validated internally by this API.
	 * For Wi-Fi Firmware, use
	 * update_and_validate_wifi_firmware()
	 * For FTFS, use
	 * update_and_validate_fs()
	 */
	int ret = update_and_validate_firmware(server_mode_data_fetch_func,
			req, filesize, p, NULL);
	if (ret == WM_SUCCESS)
		wmprintf("FW Upgrade succeeded\r\n");
	else
		wmprintf("FW Upgrade failed\r\n");
	return ret;
}
static int client_mode_upgrade_fw(char *url)
{
	http_session_t handle;
	http_resp_t *resp = NULL;
	int ret = httpc_get(url, &handle, &resp, NULL);
	if (ret != WM_SUCCESS)
		return ret;

	struct partition_entry *p = rfget_get_passive_firmware();
	/* Upgrade the mcu firmware image. Validation function passed as NULL
	 * as we are not performing any particular app level validation.
	 * Length and CRC are validated internally by this API.
	 * For Wi-Fi Firmware, use
	 * update_and_validate_wifi_firmware()
	 * For FTFS, use
	 * update_and_validate_fs()
	 */
	ret = update_and_validate_firmware(client_mode_data_fetch_func,
			(void *)handle, resp->content_length, p, NULL);
	if (ret == WM_SUCCESS)
		wmprintf("FW Upgrade succeeded\r\n");
	else
		wmprintf("FW Upgrade failed\r\n");
	http_close_session(&handle);
	return ret;
}
#endif /* !APPCONFIG_MID_LEVEL_FW_UPGRADE */

#ifdef APPCONFIG_HIGH_LEVEL_FW_UPGRADE
static int server_mode_upgrade_fw(httpd_request_t *req, int filesize)
{
	struct partition_entry *p = rfget_get_passive_firmware();
	/* Same API can be used for Wi-Fi firmware as well as FTFS
	 * by providing appropriate partition pointers.
	 * APIs like rfget_get_passive_wifi_firmware() and
	 * part_get_passive_partition_by_name() can be used to
	 * get the passive partitions of the appropriate components.
	 */
	int ret = rfget_server_mode_update(req->sock, filesize, p);
	if (ret == WM_SUCCESS)
		wmprintf("FW Upgrade succeeded\r\n");
	else
		wmprintf("FW Upgrade failed\r\n");
	return ret;
}
static int client_mode_upgrade_fw(char *url)
{
	struct partition_entry *p = rfget_get_passive_firmware();
	/* Same API can be used for Wi-Fi firmware as well as FTFS
	 * by providing appropriate partition pointers.
	 * APIs like rfget_get_passive_wifi_firmware() and
	 * part_get_passive_partition_by_name() can be used to
	 * get the passive partitions of the appropriate components.
	 */
	int ret = rfget_client_mode_update(url, p, NULL);
	if (ret == WM_SUCCESS)
		wmprintf("FW Upgrade succeeded\r\n");
	else
		wmprintf("FW Upgrade failed\r\n");
	return ret;
}
#endif /* !APPCONFIG_HIGH_LEVEL_FW_UPGRADE */

#define NUM_TOKENS	5
static int fw_url_handler(httpd_request_t *req)
{
	char buf[128];
	char fw_url[100];
	int ret;

	ret = httpd_get_data(req, buf, sizeof(buf));
	if (ret < 0) {
		wmprintf("Failed to get post request data\r\n");
		goto end;
	}
	/* Parse the JSON data */
	jsontok_t json_tokens[NUM_TOKENS];
	jobj_t jobj;
	ret = json_init(&jobj, json_tokens, NUM_TOKENS,
			buf, req->body_nbytes);
	if (ret != WM_SUCCESS)
		goto end;

	ret = json_get_val_str(&jobj, "url", fw_url, sizeof(fw_url));
	if (ret != WM_SUCCESS) {
		wmprintf("Failed to get URL\r\n");
		goto end;
	}
	led_fast_blink(board_led_1());
	ret = client_mode_upgrade_fw(fw_url);

end:
	if (ret == WM_SUCCESS) {
		led_on(board_led_1());
		ret = httpd_send_response(req, HTTP_RES_200,
				HTTPD_JSON_SUCCESS,
			strlen(HTTPD_JSON_SUCCESS), HTTP_CONTENT_JSON_STR);
		reboot_device();
	} else {
		led_off(board_led_1());
		ret = httpd_send_response(req, HTTP_RES_200,
				HTTPD_JSON_ERROR,
			strlen(HTTPD_JSON_ERROR), HTTP_CONTENT_JSON_STR);
	}
	return ret;
}

static int fw_data_handler(httpd_request_t *req)
{
	char buf[512];
	int ret = httpd_parse_hdr_tags(req, req->sock, buf,
			sizeof(buf));
	if (ret != WM_SUCCESS) {
		wmprintf("Failed to parse HTTP headers\r\n");
		goto end;
	}
	wmprintf("Received File length = %d\r\n", req->body_nbytes);
	led_fast_blink(board_led_1());
	ret = server_mode_upgrade_fw(req, req->body_nbytes);
end:
	if (ret == WM_SUCCESS) {
		led_on(board_led_1());
		ret = httpd_send_response(req, HTTP_RES_200,
				HTTPD_JSON_SUCCESS,
				strlen(HTTPD_JSON_SUCCESS),
				HTTP_CONTENT_JSON_STR);
		/* The new firmware will be loaded on reboot */
		reboot_device();
	} else {
		led_off(board_led_1());
		ret = httpd_send_response(req, HTTP_RES_200,
				HTTPD_JSON_ERROR,
				strlen(HTTPD_JSON_ERROR),
				HTTP_CONTENT_JSON_STR);
	}
	return ret;
}
struct httpd_wsgi_call secure_fw_upgrade_http_handlers[] = {
	{"/fw_data", HTTPD_DEFAULT_HDR_FLAGS, 0,
	NULL, fw_data_handler, NULL, NULL},
	{"/fw_url", HTTPD_DEFAULT_HDR_FLAGS, 0,
	NULL, fw_url_handler, NULL, NULL},
};

static int secure_fw_upgrade_http_handlers_no =
	sizeof(secure_fw_upgrade_http_handlers) /
	sizeof(struct httpd_wsgi_call);

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

/*
 * Register Web-Service handlers
 *
 */
int register_httpd_handlers()
{
	return httpd_register_wsgi_handlers(secure_fw_upgrade_http_handlers,
		secure_fw_upgrade_http_handlers_no);
}

/* This is the main event handler for this project. The application framework
 * calls this function in response to the various events in the system.
 */
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
			led_slow_blink(board_led_2());
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
		} else {
			led_fast_blink(board_led_2());
			app_sta_start();
		}

		break;
	case AF_EVT_NORMAL_CONNECTED:
		/* Start HTTP Server */
		app_httpd_only_start();
		/*
		 * Register secure FW upgrade http handlers
		 */
		register_httpd_handlers();
		led_on(board_led_2());

		char ip[16];
		app_network_ip_get(ip);
		wmprintf("Connected to Home Network with IP address = %s\r\n",
				ip);
		wmprintf("1. For Client mode firmware upgrade,"
				" use this on Host side:\r\n");
		wmprintf("curl -d '{\"url\":\"<link/to/fw.bin\"}'"
				" http://<device_ip>/fw_url\r\n");
		wmprintf("2. For Server mode firmware upgrade,"
				" use this on Host side:\r\n");
		wmprintf("curl --data-binary @fw.bin "
				"http://<device_ip>/fw_data\r\n");
		wmprintf("To reset network configurations:\r\n");
		wmprintf("psm-set network configured 0\r\n");
		wmprintf("pm-reboot\r\n");
		break;
	default:
		break;
	}
	return 0;
}

static void modules_init()
{
	int ret;

	ret = wmstdio_init(UART0_ID, 0);
	if (ret != WM_SUCCESS) {
		/* Could not init standard i/o. No prints will be displayed
		 */
		critical_error(-CRIT_ERR_APP, NULL);
	}

	ret = cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: cli_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	/*
	 * Initialize Power Management Subsystem
	 */
	ret = pm_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: pm_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	/*
	 * Register Power Management CLI Commands
	 */
	ret = pm_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: pm_cli_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	ret = gpio_drv_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: gpio_drv_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	return;
}

int main()
{
	modules_init();

	wmprintf("Build Time: " __DATE__ " " __TIME__ "\r\n");
#ifdef APPCONFIG_MID_LEVEL_FW_UPGRADE
	wmprintf("######################################################\r\n");
	wmprintf("Mid Level (Component Level) APIs Firmware Upgrade Demo\r\n");
	wmprintf("######################################################\r\n");
#endif
#ifdef APPCONFIG_HIGH_LEVEL_FW_UPGRADE
	wmprintf("##################################################\r\n");
	wmprintf("High Level (HTTP Level) APIs Firmware Upgrade Demo\r\n");
	wmprintf("##################################################\r\n");
#endif
	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		wmprintf("Failed to start application framework\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	return 0;
}
