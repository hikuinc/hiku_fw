/*
 *  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* app_psm.c: This file takes care of maintaining relevant information about the
 * device into psm.
 */

#include <string.h>
#include <stdlib.h>

#include <wmassert.h>
#include <wmstdio.h>
#include <wm_utils.h>
#include <psm.h>
#include <psm-v2.h>
#include <partition.h>
#include <app_framework.h>
#include <boot_flags.h>

#include "app_psm.h"
#include "app_cli.h"
#include "app_dbg.h"

psm_hnd_t app_psm_hnd;

psm_hnd_t psm_get_handle()
{
	if (!app_psm_hnd)
		app_w("PSM handle is NULL");
	return app_psm_hnd;
}

/* Every device has a name associated with it. This name is stored within the
 * psm. This function retrieves this name.
 */
int app_get_device_name(char *dev_name, int len)
{
	ASSERT(dev_name != NULL);

	int ret = psm_get_variable_str(app_psm_hnd, VAR_SYS_DEVICE_NAME,
				       dev_name, len);
	if (ret < 0)
		return ret;

	return WM_SUCCESS;
}

/* Every device has a name associated with it. This name is stored within the
 * psm. This function sets a new name within the device.
 */
int app_set_device_name(char *dev_name)
{
	ASSERT(dev_name != NULL);

	return psm_set_variable_str(app_psm_hnd, VAR_SYS_DEVICE_NAME,
				    dev_name);
}

/* This function is specifically required to initialize the device name if it
 * isn't already set.
 */
int app_init_device_name(char *dev_name)
{
	ASSERT(dev_name != NULL);

	char tmp[64];

	int ret = app_get_device_name(tmp, sizeof(tmp));
	if (ret != WM_SUCCESS) {
		/* Device name does not exists, lets initialize in psm */
		return app_set_device_name(dev_name);
	}

	return WM_SUCCESS;
}

/* Every device has a alias associated with it. This alias is stored within the
 * psm. This function retrieves this alias.
 */
int app_get_device_alias(char *alias, int len)
{
	ASSERT(alias != NULL);

	int ret = psm_get_variable_str(app_psm_hnd, VAR_SYS_DEVICE_ALIAS,
				       alias, len);
	if (ret < 0)
		return ret;

	return WM_SUCCESS;
}

/* Every device has a alias associated with it. This alias is stored within the
 * psm. This function sets a new alias within the device.
 */
int app_set_device_alias(char *alias)
{
	ASSERT(alias != NULL);

	return psm_set_variable_str(app_psm_hnd, VAR_SYS_DEVICE_ALIAS,
				    alias);

}

/* This function is specifically required to initialize the alias if it
 * isn't already set.
 */
int app_init_device_alias(char *alias)
{
	char tmp[64];

	int ret = app_get_device_alias(tmp, sizeof(tmp));
	if (ret != WM_SUCCESS) {
		/* Device name does not exists, lets initialize in psm */
		return app_set_device_alias(alias);
	}
	return WM_SUCCESS;
}

int app_psm_init(void)
{
	static bool psm_init_done;
	int ret;
#ifdef CONFIG_SECURE_PSM
	psm_cfg_t psm_cfg;
#endif /* CONFIG_SECURE_PSM */
	if (psm_init_done)
		return WM_SUCCESS;

	flash_desc_t fl;
	ret = part_get_desc_from_id(FC_COMP_PSM, &fl);
	if (ret != WM_SUCCESS) {
		app_d("Unable to get fdesc from id");
		return ret;
	}

#ifdef CONFIG_SECURE_PSM
	memset(&psm_cfg, 0, sizeof(psm_cfg_t));
	psm_cfg.secure = true;
	ret = psm_module_init(&fl, &app_psm_hnd, &psm_cfg);
#else
	ret = psm_module_init(&fl, &app_psm_hnd, NULL);
#endif /* CONFIG_SECURE_PSM */
	if (ret) {
		app_e("app_psm: failed to init PSM,"
			"status code %d", ret);
		return -WM_FAIL;
	}

	psm_init_done = true;

#ifdef CONFIG_APP_FRM_CLI_HISTORY
	app_psm_cli_hist_init();
#endif

	return WM_SUCCESS;
}

