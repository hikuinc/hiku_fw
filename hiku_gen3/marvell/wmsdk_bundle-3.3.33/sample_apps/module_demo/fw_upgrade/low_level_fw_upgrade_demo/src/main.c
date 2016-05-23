/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* Application demonstrating low level firmware upgrades
 *
 * Summary:
 *
 * This application showcases the use of low level firmware upgrade APIs.
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
 * To upgrade the firmware, execute the following from the host side:
 * curl --data-binary @fw.bin http://<device_ip>/fw_data
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

static int data_fetch_func(httpd_request_t *req, void *buf, uint32_t max_len)
{
	int size_read = httpd_get_data(req, buf, max_len);
	if (size_read < 0) {
		wmprintf("Could not read data from stream\r\n");
		return -WM_FAIL;
	}
	return size_read;
}
static int upgrade_fw(httpd_request_t *req, int filesize)
{
	update_desc_t ud;
	struct partition_entry *p = rfget_get_passive_firmware();
	if (filesize > p->size)
		return -WM_FAIL;
	/* Step 1. Initialize RFGET */
	rfget_init();
	/* Step 2. Begin the RFGET based update */
	rfget_update_begin(&ud, p);
	char buf[1024];
	int buf_len = sizeof(buf);
	int len, recv_filesize = 0;
	/* Read buf_len number of bytes from the HTTP client */
	while (((len = data_fetch_func(req, buf, buf_len)) > 0)
			&& (filesize > 0)) {
		/* Update received file size */
		recv_filesize += len;
		/* Update size remaining */
		filesize -= len;
		/* If buf_len is greater than the size remaining,
		 * reset the buf_len so that only the required
		 * number of bytes are read
		 */
		if (buf_len > filesize)
			buf_len = filesize;
		/* Step 3. Write the Update data */
		rfget_update_data(&ud, buf, len);
	}
	wmprintf("Written data of size %d\r\n", recv_filesize);
	/* Step 4. Verify the written firmware image */
	if (verify_load_firmware(p->start, recv_filesize) != 0) {
		wmprintf("FW Upgrade failed\r\n");
		/* Step 5(a). Abort the update and return */
		rfget_update_abort(&ud);
		return -WM_FAIL;
	}
	wmprintf("FW Upgrade succeeded\r\n");
	/* Step 5(b). Complete the Update Procedure.
	 * This changes the active partition index */
	rfget_update_complete(&ud);
	return WM_SUCCESS;
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
	ret = upgrade_fw(req, req->body_nbytes);
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
		wmprintf("To test firmware upgrade,"
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
	wmprintf("###############################\r\n");
	wmprintf("Low Level Firmware Upgrade Demo\r\n");
	wmprintf("###############################\r\n");

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		wmprintf("Failed to start application framework\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	return 0;
}
