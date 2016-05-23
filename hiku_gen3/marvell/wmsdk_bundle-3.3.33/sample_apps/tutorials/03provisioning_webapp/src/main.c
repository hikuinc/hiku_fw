/*
 *  Copyright (C) 2008-2016 Marvell International Ltd.
 *  All Rights Reserved.
 */
/* Tutorial 3: Provisioning with WebApp
 *
 * In the EZConnect provisioning, also enable the Web Server and the FTFS so
 * that web-based provisioning can also be performed as a fallback option.
 */
#include <wmstdio.h>
#include <cli.h>
#include <psm.h>
#include <app_framework.h>

/* Handle Critical Error Handlers */
void critical_error_handler(void *data)
{
	while (1)
		;
	/* do nothing -- stall */
}

/*
 * Handler invoked on WLAN_INIT_DONE event.
 */
static void event_wlan_init_done(void *data)
{
	/* We receive provisioning status in data */
	int provisioned = (int)data;

	wmprintf("Event: WLAN_INIT_DONE provisioned=%d\r\n", provisioned);

	if (provisioned) {
		wmprintf("Starting station\r\n");
		app_sta_start();
	} else {
		wmprintf("Not provisioned\r\n");
#define PROV_SSID   "Smart Plug"
#define PROV_PIN    "device_pin"
		app_ezconnect_provisioning_start(PROV_SSID,
			 (unsigned char *)PROV_PIN, strlen(PROV_PIN));

		if (app_httpd_only_start() != WM_SUCCESS)
			wmprintf("Error: Failed to start HTTPD");

#define FTFS_API_VERSION    100
#define FTFS_PART_NAME	    "ftfs"
		static struct fs *fs_handle;
		app_ftfs_init(FTFS_API_VERSION, FTFS_PART_NAME, &fs_handle);
	}
}

/*
 * Event: PROV_DONE
 *
 * Provisioning is complete. We can stop the provisioning
 * service.
 */
static void event_prov_done(void *data)
{
	wmprintf("Provisioning Completed");
	app_ezconnect_provisioning_stop();
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
	case AF_EVT_PROV_DONE:
		event_prov_done(data);
		break;
	default:
		break;
	}

	return WM_SUCCESS;
}

int main()
{
	wmstdio_init(UART0_ID, 0);

	cli_init();

	psm_cli_init();


	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		wmprintf("Failed to start application framework");
		critical_error_handler((void *) -WM_FAIL);
	}

	return 0;
}
