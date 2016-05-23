/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Simple Application using wlan_set_cal_data() API.

 * Summary:
 *
 * This application shows usage of wlan_get_cal_data feature
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
 * Handler invoked when WLAN subsystem is ready.
 *
 */
void event_wlan_init_done(void *data)
{
	wlan_cal_data_t wlan_cal_data;

	wlan_get_cal_data(&wlan_cal_data);

	wmprintf("###############################################\r\n");

	wmprintf("Calibration Data from WLAN firmware\r\n");

	dump_hex(wlan_cal_data.data, wlan_cal_data.data_len);

	wmprintf("###############################################\r\n");
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
	return;
}

int main()
{
	uint8_t cal_data[] = {0x00, 0x40, 0x02, 0x0d, 0x00, 0x00, 0x00, 0xc0,
		0x33, 0x00, 0x20, 0x00, 0xb8, 0x04, 0x00, 0x00, 0x00, 0x11,
		0x00, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x80, 0x00,
		0x00, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x02, 0x00, 0x00, 0x0b, 0x02, 0x00, 0x00, 0x1f,
		0x02, 0x01, 0x00, 0x0a, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0xcf, 0x1f,
		0xff, 0xff, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	modules_init();

	dbg("Build Time: " __DATE__ " " __TIME__ "");

	wmprintf("###############################################\r\n");

	wmprintf("Set Calibration data to WLAN firmware:\r\n");

	dump_hex(cal_data, sizeof(cal_data));

	wmprintf("###############################################\r\n");

	wlan_set_cal_data(cal_data, sizeof(cal_data));

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		dbg("Failed to start application framework");
		critical_error(-CRIT_ERR_APP, NULL);
	} else {
		dbg("app_framework started\n");
	}
	return 0;
}
