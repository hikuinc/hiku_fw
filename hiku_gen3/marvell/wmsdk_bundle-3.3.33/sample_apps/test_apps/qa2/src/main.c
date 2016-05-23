/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Application to verify the PSM module functionality
 *
 * Summary:
 *
 * This application is created to test the functionality of PSM module using
 * 'psm-test' CLI
 */

#include <wmstdio.h>
#include <wm_os.h>
#include <nw_utils.h>
#include <board.h>
#include <wmtime.h>
#include <cli.h>
#include <mdev_pm.h>
#include <psm.h>
#include "aes_test_main.h"

int psm_test_cli_init();

int modules_init()
{
	int ret = wmstdio_init(UART0_ID, 0);

	ret = cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("cli init failed: %d\r\n", ret);
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
	ret = aes_test_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("aes_test_cli_init failed: %d\r\n", ret);
		return ret;
	}
	ret = psm_test_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("psm_test_cli_init failed: %d\r\n", ret);
		return ret;
	}

	return WM_SUCCESS;
}

static void cmd_app_name(int argc, char *argv[])
{
	wmprintf("qa2\r\n");
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

	ret = modules_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("modules cli init failed: %d\r\n", ret);
		return ret;
	}
	wmprintf("QA2 Test app started. AES and PSM test CLIs are available" \
			"\r\n");
	ret = app_name_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("app name cli init failed: %d\r\n", ret);
		return ret;
	}
	return WM_SUCCESS;
}
