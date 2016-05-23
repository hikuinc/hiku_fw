/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Simple Webhandler Application using micro-AP
 *
 * Summary:
 *
 * This application starts a micro-AP network.
 * It also starts a Web Server and makes available a web URI
 * http://192.168.10.1/hello.
 *
 * Description:
 *
 * The application is written using Application Framework that
 * simplifies development of WLAN networking applications.
 *
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
 * This app registers following handlers:
 * 1. Hello get handler
 * 2. Hello post handler
 * 3. LED state get handler
 * 4. LED state post handler
 *
 * Sample commands for above mentioned handlers are as follows:
 * 1. curl http://192.168.10.1/hello
 * 2. curl -d 'People' http://192.168.10.1/hello
 * 3. curl http://192.168.10.1/led-state
 * 4. curl -d '{"state":1}' http://192.168.10.1/led-state
 *
 * Expected outputs for above commands are as follows:
 * 1. Hello World
 * 2. Hello People
 * 3. {"state":0}
 * 4. {"success":0} (LED 1 will be set)
 *
 *
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
#include <critical_error.h>

extern int register_httpd_json_handlers();
extern int register_httpd_text_handlers();
/*------------------Macro Definitions ------------------*/

appln_config_t appln_cfg = {
	.passphrase = "marvellwm",
	.ssid = "WebHandler-demo"
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
 *
 */

void event_uap_started(void *data)
{
	dbg("mdns uap up event");
	int ret;
	/* Start http server */
	ret = app_httpd_start();
	if (ret != WM_SUCCESS)
		dbg("Failed to start HTTPD");

	ret = register_httpd_json_handlers();
	if (ret != WM_SUCCESS)
		dbg("Failed to register HTTPD JSON handlers");
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
		dbg("Error: wmstdio_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	return;
}

int main()
{
	modules_init();

	dbg("Build Time: " __DATE__ " " __TIME__ "");
	wmprintf("Simple Webhandler Application using micro-AP\r\n");
	wmprintf("------------------------------------\r\n");
	wmprintf("Connect Client to WebHandler-demo network\r\n");
	wmprintf("Passphrase for connection : marvellwm \r\n");
	wmprintf("1. curl http://192.168.10.1/hello \r\n");
	wmprintf("2. curl -d 'People' http://192.168.10.1/hello \r\n");
	wmprintf("3. curl http://192.168.10.1/led-state\r\n");
	wmprintf("4. curl -d '{\"state\":1}'"
		 "http://192.168.10.1/led-state\r\n");
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
