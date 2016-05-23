/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* Application demonstrating secure firmware upgrades
 *
 * Summary:
 *
 * This application showcases the ED25519-Chacha20 and RSA-AES
 * firmware upgrade support in http server mode as well as http
 * client mode.
 *
 * Description:
 *
 * In order to use firmware upgrades, the device first needs to be provisioned.
 * To keep the app simple, provisioning needs to be done using CLI. The
 * appropriate commands will be shown on the console on boot-up.
 *
 * When the device connects to a network, the IP address will be displayed
 * on the console. This can be then used to demonstrate secure firmware
 * upgrade.
 *
 * By default, ED25519-Chacha20 based upgrades are enabled. This can be
 * changed to RSA-AES by changing the config option in
 * secure_fw_upgrade_demo/config.mk. You will also have to enable the support
 * in WMSDK by setting CONFIG_FWUPG_RSA_AES=y in your .config or
 * enabling it using make menuconfig and selecting it under
 * Modules -> Firmware Upgrades
 *
 * Two upgrade APIs are exposed by this app.
 * 1. Client mode firmware upgrades
 * curl -d '{"url":"http://<server_ip>/link/to/fw.upg"}' \
 *	http://<device_ip>/fw_url
 * - The secure upgrade image is fetched from the remote server based on
 *   the url.
 *
 * 2. Server mode firmware upgrades
 * curl --data-binary @fw.upg http://<device_ip>/fw_data
 * - The secure upgrade image is POSTed directly to the device
 *
 * On successful upgrade, {"success":0} will be returned and device will
 * reboot.
 * On failure, {"error":-1} will be returned.
 *
 * For details on steps to generate the secure upgrade image, refer to the
 * README.ed_chacha and README.rsa_aes files in
 * sdk/tools/host-tools/fw_generator/
 * The config files to be used for fw_generator have been added as
 * fwupg.ed_chacha.config and fwupg.rsa_aes.config in this app's root folder.
 *
 * New configurations should be generated and used for production samples.
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

static void app_reboot_cb()
{
	ll_printf("Rebooting...");
	pm_reboot_soc();
}

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

static int server_mode_data_fetch_cb(void *priv, void *buf, uint32_t max_len)
{
	httpd_request_t *req = (httpd_request_t *)priv;
	int size_read = httpd_get_data(req, buf, max_len);
	if (size_read < 0) {
		wmprintf("Server mode: Could not read data from stream\r\n");
		return -WM_FAIL;
	}
	return size_read;
}
#ifdef APPCONFIG_FWUPG_ED_CHACHA
#include <fw_upgrade_ed_chacha.h>
static const uint8_t fwupg_verification_key[] =
	"cfedc540a8f11a6dbd58148570cda63b6d194367b5166b85ce8fc711b5cff680";
static const uint8_t fwupg_encrypt_decrypt_key[] =
	"12f145a764fb412b6a0f9eecbc7c831df585816d267e47dfd636bd9d3bf94bdf";

static int client_mode_upgrade_fw(char *url)
{
	wmprintf("Using ED-Chacha client mode upgrade\r\n");
	uint8_t decrypt_key[ED_CHACHA_DECRYPT_KEY_LEN];
	uint8_t verification_key[ED_CHACHA_VERIFICATION_KEY_LEN];
	hex2bin(fwupg_verification_key, verification_key,
			sizeof(verification_key));
	hex2bin(fwupg_encrypt_decrypt_key, decrypt_key,
			sizeof(decrypt_key));
	return ed_chacha_client_mode_upgrade_fw(FC_COMP_FW,
			url, verification_key, decrypt_key, NULL);
}
static int server_mode_upgrade_fw(httpd_request_t *req)
{
	wmprintf("Using Ed-Chacha server mode upgrade\r\n");
	uint8_t decrypt_key[ED_CHACHA_DECRYPT_KEY_LEN];
	uint8_t verification_key[ED_CHACHA_VERIFICATION_KEY_LEN];
	hex2bin(fwupg_verification_key, verification_key,
			sizeof(verification_key));
	hex2bin(fwupg_encrypt_decrypt_key, decrypt_key,
			sizeof(decrypt_key));
	return ed_chacha_server_mode_upgrade_fw(FC_COMP_FW,
			verification_key, decrypt_key,
			server_mode_data_fetch_cb, req);

}
#endif /* !APPCONFIG_FWUPG_ED_CHACHA */

#ifdef APPCONFIG_FWUPG_RSA_AES
#include <fw_upgrade_rsa_aes.h>
static const uint8_t fwupg_verification_key[] =
	"30820122300d06092a864886f70d01010105000382010f003082010a028201010"
	"0dd453e4cfabe1c9c8b2697d1c39daadbaefc83d578e474b27c69c38218f96aa0"
	"c81919b6acf9dfeeb9fd0e01af94bb08207bd6de794c7c6fa6993b8e5b5085bb6"
	"a5345b2e19b4466d1af70da3ef94a73facdeb54d4819ec87b8fe8b2558cb6f916"
	"293da00b8e667ce73f671436b53ec819a1d22d241495fa3ef5498c6433f86bc0a"
	"fbf0f1cd884e800e3c7a582db5c88ba1c74747f018f558a5507f27405ec2b268f"
	"04e10ef659eb89f71f9951862530c9fc5c5c8c8da0824d73009ef5339a38bb100"
	"aad0e5890095905d18e46d8c4830f6397524923e8b0e98e03a13dad09333b8b1d7"
	"a691967f156fd315dfce369d56f2a9b9bcdcbeb7f4479442b6a3307ab0203010001";
static const uint8_t fwupg_encrypt_decrypt_key[] =
	"aae4e447569e1b2ec1a1ede4c2e616f9";

static int client_mode_upgrade_fw(char *url)
{
	wmprintf("Using RSA-AES client mode upgrade\r\n");
	uint8_t decrypt_key[AES_KEY_LEN];
	uint8_t verification_key[RSA_PUBLIC_KEY_LEN];
	hex2bin(fwupg_verification_key, verification_key,
			sizeof(verification_key));
	hex2bin(fwupg_encrypt_decrypt_key, decrypt_key,
			sizeof(decrypt_key));
	return rsa_aes_client_mode_upgrade_fw(FC_COMP_FW,
			url, verification_key, decrypt_key,
			NULL);
}
static int server_mode_upgrade_fw(httpd_request_t *req)
{
	wmprintf("Using RSA-AES server mode upgrade\r\n");
	uint8_t decrypt_key[AES_KEY_LEN];
	uint8_t verification_key[RSA_PUBLIC_KEY_LEN];
	hex2bin(fwupg_verification_key, verification_key,
			sizeof(verification_key));
	hex2bin(fwupg_encrypt_decrypt_key, decrypt_key,
			sizeof(decrypt_key));
	return rsa_aes_server_mode_upgrade_fw(FC_COMP_FW,
			verification_key, decrypt_key,
			server_mode_data_fetch_cb, req);

}
#endif /* !APPCONFIG_FWUPG_RSA_AES */

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
	if (ret != WM_SUCCESS) {
		goto end;
	}
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
	ret = server_mode_upgrade_fw(req);
end:
	if (ret == WM_SUCCESS) {
		led_on(board_led_1());
		ret = httpd_send_response(req, HTTP_RES_200,
				HTTPD_JSON_SUCCESS,
				strlen(HTTPD_JSON_SUCCESS),
				HTTP_CONTENT_JSON_STR);
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
	{"/fw_url", HTTPD_DEFAULT_HDR_FLAGS, 0,
	NULL, fw_url_handler, NULL, NULL},
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
		wmprintf("1. For Client mode firmware upgrade,"
				" use this on Host side:\r\n");
		wmprintf("curl -d '{\"url\":\"<link/to/fw.upg\"}'"
				" http://<device_ip>/fw_url\r\n");
		wmprintf("2. For Server mode firmware upgrade,"
				" use this on Host side:\r\n");
		wmprintf("curl --data-binary @fw.upg "
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
	wmprintf("############################\r\n");
	wmprintf("Secure Firmware Upgrade Demo\r\n");
	wmprintf("############################\r\n");

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		wmprintf("Failed to start application framework\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	return 0;
}
