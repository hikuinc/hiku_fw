/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Simple Application using wm_core_and_wlan_init(), wm_wlan_deinit()
 * APIs to demonstrate WLAN PDN functionality.
 *
 * Summary:
 *
 * An application to  demonstrate Power down feature of WLAN
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
 * After WLAN Initialization, two clis wlan-pdn-enter and wlan-pdn-exit
 * are provided to test WLAN PDN functionality.
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
#include <wm_wlan.h>
#include <cli.h>
#include <critical_error.h>

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

static void test_wlan_pdn_enter()
{
	wm_wlan_deinit();

	CLK_RC32M_SfllRefClk();

	PMU_PowerDownWLAN();

	wmprintf("Entered WLAN PDN mode\r\n");

}

static void test_wlan_pdn_exit()
{
	PMU_PowerOnWLAN();

	os_thread_sleep(os_msec_to_ticks(50));

	wm_wlan_init();

	wmprintf("Exit WLAN PDN mode\r\n");
}

static struct cli_command wlan_pdn_commands[] = {
	{"wlan-pdn-enter", NULL, test_wlan_pdn_enter},
	{"wlan-pdn-exit", NULL, test_wlan_pdn_exit},
};

int wlan_pdn_cli_init(void)
{
	int i;

	for (i = 0;	i < sizeof(wlan_pdn_commands) \
		/ sizeof(struct cli_command); i++)
		if (cli_register_command(&wlan_pdn_commands[i]))
			return -WM_FAIL;

	return WM_SUCCESS;
}

/*
 * Handler invoked when WLAN subsystem is ready.
 *
 */
void event_wlan_init_done(void *data)
{
	int ret;

	dbg("WLAN init done");

	ret = wlan_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_cli_init failed");

	ret = wlan_pdn_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_pdn_cli_init failed");
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

	ret = cli_init();

	if (ret != WM_SUCCESS) {
		dbg("Error: cli init failed.");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	return;
}

int main()
{
	modules_init();

	dbg("Build Time: " __DATE__ " " __TIME__ "");

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		dbg("Failed to start application framework");
		critical_error(-CRIT_ERR_APP, NULL);
	} else {
		dbg("app_framework started\n");
	}
	return 0;
}
