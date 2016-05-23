/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Application to demonstrate WLAN and network CLI's and their functionality
 *
 * Summary:
 *
 * This application is created to demonstrate the functionality of:
 *   1. wlan, iw*, uaputl CLI's
 *   2. nw_utils (ping) CLI's
 *   3. TTCP (performance measurement)
 */

#include <wmstdio.h>
#include <wm_os.h>
#include <nw_utils.h>
#include <wm_net.h>
#include <board.h>
#include <wm_wlan.h>
#include <wmtime.h>
#include <cli.h>
#include <mdev_pm.h>
#include <dhcp-server.h>
#include <ttcp.h>

static int wlan_event_callback(enum wlan_event_reason event, void *data)
{
	/* WLAN Event Handling */
	return 0;
}

int modules_init()
{
	int ret = wmstdio_init(UART0_ID, 0);

	ret = cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("cli init failed: %d\r\n", ret);
		return ret;
	}

	ret = pm_init();
	if (ret != WM_SUCCESS) {
		wmprintf("pm init failed: %d\r\n", ret);
		return ret;
	}

	return WM_SUCCESS;
}
int modules_cli_init()
{
	int ret;
	ret = wmtime_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("wmtime cli init failed: %d\r\n", ret);
		return ret;
	}

	ret = dhcpd_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("dhcpd cli init failed: %d\r\n", ret);
		return ret;
	}

	ret = pm_mcu_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("pm_mcu cli init failed: %d\r\n", ret);
		return ret;
	}

	ret = pm_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("pm cli init failed: %d\r\n", ret);
		return ret;
	}

	ret = wlan_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("wlan cli init failed: %d\r\n", ret);
		return ret;
	}

	ret = wlan_iw_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("iw cli init failed: %d\r\n", ret);
		return ret;
	}

	ret = wlan_uaputl_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("uaputl cli init failed: %d\r\n", ret);
		return ret;
	}

	ret = wlan_pm_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("wlan_pm cli init failed: %d\r\n", ret);
		return ret;
	}

	ret = nw_utils_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("nw_utils cli init failed: %d\r\n", ret);
		return ret;
	}

	ret = ttcp_init();
	if (ret != WM_SUCCESS) {
		wmprintf("ttcp init failed: %d\r\n", ret);
		return ret;
	}

	return WM_SUCCESS;
}

static void cmd_app_name(int argc, char *argv[])
{
	wmprintf("qa1\r\n");
}


static struct cli_command commands[] = {
	{"app-name", NULL, cmd_app_name},
};

int app_name_cli_init(void)
{
	int i;

	for (i = 0; i < sizeof(commands) / sizeof(struct cli_command); i++)
		if (cli_register_command(&commands[i]))
			return 1;
	return 0;
}

/*
 * The application entry point is main(). At this point, minimal
 * initialization of the hardware is done, and also the RTOS is
 * initialized.
 */
int main(void)
{
	int ret;
	ret = modules_init();
	if (ret != WM_SUCCESS) {
		wmprintf("module init failed: %d\r\n", ret);
		return ret;
	}

	ret = wm_core_and_wlan_init();
	if (ret != WM_SUCCESS) {
		wmprintf("wm core and wlan init failed: %d\r\n", ret);
		return ret;
	}

	ret = wlan_start(wlan_event_callback);
	if (ret != WM_SUCCESS) {
		wmprintf("wlan_start failed: %d\r\n", ret);
		return ret;
	}

	ret = modules_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("modules cli init failed: %d\r\n", ret);
		return ret;
	}

	ret = app_name_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("app name cli init failed: %d\r\n", ret);
		return ret;
	}
	return WM_SUCCESS;
}
