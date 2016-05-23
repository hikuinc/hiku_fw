/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Simple HTTP Secure Server Application using micro-AP
 *
 * Summary:
 *
 * This application starts a micro-AP network.
 * It also starts a Secure Web Server and makes available a web URI
 * https://192.168.10.1/ssl-test.
 *
 * Description:
 *
 * The application is written using Application Framework that
 * simplifies development of WLAN networking applications.
 *
 * WLAN Initialization:
 *
 * When the application framework is started, it starts up the WLAN
 * sub-system and initializes the network stack. The app receives the event
 * when the WLAN subsystem has been started and initialized.
 *
 * The app starts a micro-AP network along with a DHCP server. This will
 * create a WLAN network and also creates a IP network interface
 * associated with that WLAN network. The DHCP server allows devices
 * to associate and connect to this network and establish an IP level
 * connection.
 *
 * When the network is initialized and ready, app will receive a
 * Micro-AP started event.
 *
 * Micro-AP Started:
 *
 * The micro-AP network is now up and ready.
 *
 * The app starts the web-server which will then listen to incoming http
 * connections (e.g, from a browser).
 *
 * By default, there are no handlers defined, and any request to the
 * web-server will result in a 404 error from the web-server.
 *
 * This app registes following handler:
 *  /ssl-test
 * Connect a device to the Micro-AP interface "Secure-HTTPD"
 *
 * Open a web browser and try opening
 *  https://192.168.10.1/ssl-test
 *
 * Expected outputs for above is as follows:
 *  "SSL Test passed"
 *
 */
#include <wm_os.h>
#include <app_framework.h>
#include <wmstdio.h>
#include <wm_net.h>
#include <mdev_gpio.h>
#include <appln_cb.h>
#include <appln_dbg.h>
#include <httpd.h>
#include <dhcp-server.h>
#include <ftfs.h>
#include <rfget.h>
#include <server-key.pem.h>
#include <server-cert.pem.h>
#include <critical_error.h>

extern int register_httpd_text_handlers();
/*------------------Macro Definitions ------------------*/

#define EPOCH_TIME 1388534400 /* epoch value of 1 Jan 2014 00:00:00 */
appln_config_t appln_cfg = {
	.passphrase = "marvellwm",
	.ssid = "Secure-HTTPD"
};

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
 * Handler invoked when the Micro-AP Network interface
 * is ready.
 */

void event_uap_started(void *data)
{
	int ret;
#ifdef CONFIG_ENABLE_HTTPS_SERVER
	httpd_tls_certs_t httpd_tls_certs = {
		.server_cert = server_cert,
		.server_cert_size = sizeof(server_cert) - 1,
		.server_key = server_key,
		.server_key_size = sizeof(server_key) - 1,
		.client_cert = NULL,
		.client_cert_size = 0,
	};
	wmtime_time_set_posix(EPOCH_TIME);
	ret = httpd_use_tls_certificates(&httpd_tls_certs);
#endif /* APPCONFIG_HTTPS_SERVER */
	/* Start http server */
	ret = app_httpd_start();
	if (ret != WM_SUCCESS)
		dbg("Failed to start HTTPD");

	ret = register_httpd_text_handlers();
	if (ret != WM_SUCCESS)
		dbg("Failed to register HTTPD JSON handlers");
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
	app_uap_start_with_dhcp(appln_cfg.ssid, appln_cfg.passphrase);
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
	case AF_EVT_UAP_STARTED:
		event_uap_started(data);
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
		critical_error(-CRIT_ERR_APP, NULL);
	}
	return;
}

int main()
{
	modules_init();

	wmprintf("Simple HTTPD Secure Application using micro-AP\r\n");
	wmprintf("------------------------------------\r\n");
	wmprintf("Connect Client to Secure-HTTPD network\r\n");
	wmprintf("Passphrase for connection : marvellwm \r\n");
	wmprintf(" Open a web browser and try opening\r\n");
	wmprintf(" https://192.168.10.1/ssl-test \r\n");
	wmprintf("------------------------------------\r\n");

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		dbg("Failed to start application framework");
				critical_error(-CRIT_ERR_APP, NULL);
	} else {
		dbg("app_framework started\n");
	}
	return 0;
}
