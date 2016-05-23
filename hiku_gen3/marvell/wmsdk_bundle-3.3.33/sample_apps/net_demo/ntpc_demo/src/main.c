/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Simple  NTP client for time sync using UDP
 *
 * Summary:
 *
 * Sample Application demonstrating a simple time sync operation using NTP
 *
 */
#include <wm_os.h>
#include <app_framework.h>
#include <cli.h>
#include <wmstdio.h>
#include <board.h>
#include <wmtime.h>
#include <psm.h>
#include <critical_error.h>

/* Thread handle */
static os_thread_t time_sync_thread;
/* Buffer to be used as stack */
static os_thread_stack_define(time_sync_stack, 4 * 1024);

int ntpc_sync(const char *ntp_server, uint32_t max_num_pkt_xchange);

/*------------------Macro Definitions ------------------*/

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

static char *month_names[] = { "Jan", "Feb", "Mar",
				     "Apr", "May", "Jun",
				     "Jul", "Aug", "Sep",
				     "Oct", "Nov", "Dec" };
static char *day_names[] = { "Sun", "Mon", "Tue", "Wed",
				   "Thu", "Fri", "Sat" };


static void _time_sync()
{
	while (1) {
		ntpc_sync("pool.ntp.org", 2);
		struct tm c_time;
		wmtime_time_get(&c_time);
		wmprintf("%s %d %s %.2d %.2d:%.2d:%.2d\r\n",
			 day_names[c_time.tm_wday],
			 c_time.tm_year + 1900, month_names[c_time.tm_mon],
			 c_time.tm_mday, c_time.tm_hour,
			 c_time.tm_min, c_time.tm_sec);
		os_thread_sleep(os_msec_to_ticks(30000));
	}
	os_thread_self_complete(NULL);
	return;

}



static int time_sync()
{
	int ret = os_thread_create(&time_sync_thread,  /* thread handle */
				   "time-sync",/* thread name */
				   _time_sync,  /* entry function */
				   0,       /* argument */
				   &time_sync_stack,  /* stack */
				   OS_PRIO_2);     /* priority - medium low */
	return ret;
}



/* This is the main event handler for this project. The application framework
 * calls this function in response to the various events in the system.
 */
int common_event_handler(int event, void *data)
{
	int ret;
	static bool is_time_sync_started;
	switch (event) {
	case AF_EVT_WLAN_INIT_DONE:
		ret = psm_cli_init();
		if (ret != WM_SUCCESS)
			wmprintf("Error: psm_cli_init failed\r\n");
		int i = (int) data;

		if (i == APP_NETWORK_NOT_PROVISIONED) {
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
		} else
			app_sta_start();

		break;
	case AF_EVT_NORMAL_CONNECTED: {
		char ip[16];
		app_network_ip_get(ip);

		wmprintf("Connected with IP address = %s\r\n", ip);
		if (!is_time_sync_started) {

			/* Start the server after device is connected */
			time_sync();
			is_time_sync_started = true;
		}
	}
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
		wmprintf("Error: wmstdio_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	ret = cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: cli_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	ret = pm_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: pm_cli_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	/* Initialize time subsystem.
	 *
	 * Initializes time to 1/1/1970 epoch 0.
	 */
	ret = wmtime_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: wmtime_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	ret = wlan_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: wlan_cli_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	return;
}

int main()
{
	modules_init();

	wmtime_cli_init();

	wmprintf("Build Time: " __DATE__ " " __TIME__ "\r\n");

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		wmprintf("Failed to start application framework\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	return 0;
}
