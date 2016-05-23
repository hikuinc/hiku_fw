/*
 *  Copyright (C) 2008-2015 Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 * Wi-Fi Driver Application
 *
 * Summary:
 *
 * Just initialize Wi-Fi Driver module.
 *
 */
#include <wmstdio.h>
#include <wm_os.h>
#include <wmtime.h>
#include <wmlog.h>
#include <cli.h>
#include <healthmon.h>
#include <wlan_11d.h>
#include <boot_flags.h>
#include <partition.h>

#define init_e(...)                              \
	wmlog_e("init", ##__VA_ARGS__)

int app_core_init(void)
{
	int ret = 0;

	wmstdio_init(UART0_ID, 0);

	ret = cli_init();
	if (ret != WM_SUCCESS) {
		init_e("Cli init failed.");
		goto out;
	}

	ret = wmtime_init();
	if (ret != WM_SUCCESS) {
		init_e("Wmtime init failed.");
		goto out;
	}

	ret = pm_init();
	if (ret != WM_SUCCESS) {
		init_e("Power manager init failed.");
		goto out;
	}

	ret = healthmon_init();
	if (ret != WM_SUCCESS) {
		init_e("Healthmon init failed.");
		goto out;
	}

	wmlog("boot", "Reset Cause Register: 0x%x",
	      boot_reset_cause());
	if (boot_reset_cause() & (1<<5))
		wmlog("boot", " - Watchdog reset bit is set");

	/* Read partition table layout from flash */
	part_init();
out:
	return ret;
}


/*
 * The application entry point is main(). At this point, minimal
 * initialization of the hardware is done, and also the RTOS is
 * initialized.
 */
int main(void)
{
	struct partition_entry *p;
	short history = 0;
	struct partition_entry *f1, *f2;
	int ret;

	ret = app_core_init();
	if (ret != WM_SUCCESS) {
		init_e("Error: Core init failed");
		return ret;
	}

	ret = part_init();
	if (ret != WM_SUCCESS) {
		init_e("could not read partition table\r\n");
		return ret;
	}
	f1 = part_get_layout_by_id(FC_COMP_WLAN_FW, &history);
	f2 = part_get_layout_by_id(FC_COMP_WLAN_FW, &history);

	if (f1 && f2)
		p = part_get_active_partition(f1, f2);
	else if (!f1 && f2)
		p = f2;
	else if (!f2 && f1)
		p = f1;
	else {
		init_e(" Wi-Fi Firmware not detected\r\n");
		return -WM_FAIL;
	}

	flash_desc_t fl;
	part_to_flash_desc(p, &fl);

	/* Initialize Wi-Fi */
	ret = wifi_init(&fl);

	wifi_set_region_code(0x50);

	wifi_send_scan_cmd(3, NULL, NULL, 0, NULL, 0, false);

	return 0;
}
