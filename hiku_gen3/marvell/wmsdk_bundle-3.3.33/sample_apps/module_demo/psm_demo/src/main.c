/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* Application that demonstrates Persistent Storage Manager (PSM) usage
 *
 * Summary:
 *
 * This application maintains a count of how many times the application was
 * booted up. This count is maintained in PSM.
 *
 * Description:
 *
 * On bootup,
 *  - the application identifies the partition that stores the PSM data
 *  - uses this information to initialize the PSM
 *  - registers a module 'bootinfo' with the PSM, if not already present
 *  - reads and prints the variable 'bootcount' from this partition
 *  - increments the value of 'bootcount' by 1 and writes the new value to PSM
 *
 * Since the value is written to PSM, the value is maintained across system
 * reboot events.
 */
#include <stdlib.h>

#include <wmstdio.h>
#include <wm_os.h>
#include <psm.h>
#include <psm-v2.h>
#include <partition.h>

#define STR_BUF_SZ 10

/* This is an entry point for the application.
   All application specific initialization is performed here. */
int main(void)
{
	int bootcount = 0;
	int ret;
	char strval[STR_BUF_SZ] = { '\0' };

	/* Initializes console on uart0 */
	ret = wmstdio_init(UART0_ID, 0);
	if (ret == -WM_FAIL) {
		wmprintf("Failed to initialize console on uart0\r\n");
		return -1;
	}

	flash_desc_t fl;
	ret = part_get_desc_from_id(FC_COMP_PSM, &fl);
	if (ret != WM_SUCCESS) {
		wmprintf("Unable to get flash desc from id\r\n");
		return -1;
	}

	/* Initilize psm module */
	psm_hnd_t psm_hnd;
	ret = psm_module_init(&fl, &psm_hnd, NULL);
	if (ret != 0) {
		wmprintf("Failed to initialize psm module\r\n");
		return -1;
	}

	wmprintf("PSM Demo application Started\r\n");

	ret = psm_get_variable(psm_hnd, "bootinfo.bootcount",
			       strval, sizeof(strval));
	if (ret < 0) {
		wmprintf("No bootcount value found.\r\n");
		wmprintf("Initializing bootcount to zero.\r\n");
		bootcount = 0;
	} else {
		bootcount = atoi(strval);
		wmprintf("Bootcount = %d found. Incrementing it to %d\r\n", \
				bootcount, bootcount+1);
		bootcount++;
	}

	snprintf(strval, STR_BUF_SZ, "%d", bootcount);

	/* This function sets the value of a variable in psm  */
	ret = psm_set_variable(psm_hnd, "bootinfo.bootcount",
			       strval, strlen(strval));
	if (ret)
		wmprintf("Error writing value in PSM\r\n");

	psm_module_deinit(&psm_hnd);
	return ret;
}
