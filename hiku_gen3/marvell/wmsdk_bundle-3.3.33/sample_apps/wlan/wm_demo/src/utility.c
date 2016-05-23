/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* System includes */
#include <wlan.h>
#include <healthmon.h>
#include <cli.h>
#include <wmtime.h>
#include <critical_error.h>

#include "utility.h"

static uint8_t prov_key[EZCONN_MAX_KEY_LEN + 1];
static int prov_key_len;

void modules_init()
{
	int ret;

#ifdef APPCONFIG_DEBUG_ENABLE
	ret = wmstdio_init(UART0_ID, 0);
	if (ret != WM_SUCCESS) {
		dbg("Error: wmstdio_init failed");
	critical_error(-CRIT_ERR_APP, NULL);
	}
#endif /* APPCONFIG_DEBUG_ENABLE */

	/* Initialize time subsystem.
	 *
	 * Initializes time to 1/1/1970 epoch 0.
	 */
	ret = wmtime_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: wmtime_init failed");
	critical_error(-CRIT_ERR_APP, NULL);
	}

	/*
	 * Initialize Power Management Subsystem
	 */
	ret = pm_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: pm_init failed");
	critical_error(-CRIT_ERR_APP, NULL);
	}

#ifdef APPCONFIG_DEBUG_ENABLE
	ret = cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: cli_init failed");
	critical_error(-CRIT_ERR_APP, NULL);
	}
	/*
	 * Register Time CLI Commands
	 */
	ret = wmtime_cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: wmtime_cli_init failed");
	critical_error(-CRIT_ERR_APP, NULL);
	}
#endif /* APPCONFIG_DEBUG_ENABLE */

	ret = healthmon_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: healthmon_init failed");
	critical_error(-CRIT_ERR_APP, NULL);
	}

	ret = healthmon_start();
	if (ret != WM_SUCCESS) {
		dbg("Error: healthmon_start failed");
	critical_error(-CRIT_ERR_APP, NULL);
	}
	/* Set the final_about_to_die handler of the healthmon */
	healthmon_set_final_about_to_die_handler
		((void (*)())final_about_to_die);

	return;
}

int get_prov_ssid(char *uap_prefix, int sizeof_prefix,
		  char *uap_ssid, int sizeof_ssid)
{
	if ((sizeof_prefix > (IEEEtypes_SSID_SIZE - 5))) {
		dbg("Error: Invalid prefix length");
		return -WM_FAIL;
	}

	if ((sizeof_prefix > (IEEEtypes_SSID_SIZE - 5))) {
		dbg("Error: Invalid ssid length");
		return -WM_FAIL;
	}

	uint8_t my_mac[6];
	wlan_get_mac_address(my_mac);

	memset(uap_ssid, 0, sizeof_ssid);
	snprintf(uap_ssid, sizeof_ssid, "%s-%02X%02X",
		 uap_prefix, my_mac[4], my_mac[5]);
	dbg("Using %s as the uAP SSID", uap_ssid);

	return WM_SUCCESS;
}

int set_prov_key(uint8_t *key, int sizeof_key)
{
	if ((sizeof_key <  EZCONN_MIN_KEY_LEN) || (sizeof_key >
						   EZCONN_MAX_KEY_LEN)) {
		dbg("Error: Invalid provisioning key length");
		return -WM_FAIL;
	}
	memset(prov_key, 0 , sizeof_key);
	memcpy(prov_key, key, sizeof_key);
	prov_key_len = sizeof_key;
	return WM_SUCCESS;
}

int get_prov_key(uint8_t *key, int sizeof_key)
{
	if (!prov_key_len) {
		dbg("Error: Provisioning key not set");
		return -WM_FAIL;
	}

	if (sizeof_key < prov_key_len) {
		dbg("Error: Invalid key buffer length");
		return -WM_FAIL;
	}

	memcpy(key, prov_key, prov_key_len);
	return prov_key_len;
}
