/*
 * Copyright (C) 2015, Marvell International Ltd.
 * All Rights Reserved.
 */
#include <wmstdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <psm-v2.h>
#include <psm-utils.h>
#include <cli.h>
#include <partition.h>
#include <flash.h>

#include "mfg-internal.h"

static psm_hnd_t mfg_psm_hnd;
static bool mfg_psm_init_done;

int mfg_get_variable(const char *var_name, char *buf, int max_len)
{
	if (!mfg_psm_init_done)
		return -WM_FAIL;
	return psm_get_variable(mfg_psm_hnd, var_name, buf, max_len);
}

int mfg_set_variable(const char *var_name, const char *value,
	int val_len)
{
	if (!mfg_psm_init_done)
		return -WM_FAIL;
	return psm_set_variable(mfg_psm_hnd, var_name, value, val_len);
}

int mfg_psm_deinit()
{
	if (!mfg_psm_init_done)
		return WM_SUCCESS;
	/* Directly setting this to false as the psm_module_deinit() returns
	 * error only if the pointer passed to it is NULL.
	 */
	mfg_psm_init_done = false;
	return psm_module_deinit(&mfg_psm_hnd);
}


#define FULL_VAR_NAME_SIZE 64

void mfg_cli_get(int argc, char **argv)
{
	char value[64];
	char full_name[FULL_VAR_NAME_SIZE];

	if (argc != 2) {
		wmprintf("[mfg] Usage: %s <variable>\r\n", argv[0]);
		wmprintf("[mfg] Error: invalid number of arguments\r\n");
		return;
	}

	if (snprintf(full_name, sizeof(full_name),
		     "%s", argv[1]) > (FULL_VAR_NAME_SIZE - 1)) {
		wmprintf("Invalid length\r\n");
		return;
	}

	memset(value, 0, sizeof(value));
	mfg_get_variable(full_name, value, sizeof(value));
	wmprintf("%s = %s\r\n", full_name, value);
}

static void mfg_dump_psm(int argc, char **argv)
{
	psm_util_dump(mfg_psm_hnd);
}

struct cli_command mfg_psm_commands[] = {
	{"mfg-get", "<variable>", mfg_cli_get},
	{"mfg-dump", NULL, mfg_dump_psm},
};

int  mfg_cli_init()
{
	int i;
	/* Register psm commands */
	for (i = 0; i < sizeof(mfg_psm_commands) / sizeof(struct cli_command);
		i++)
		cli_register_command(&mfg_psm_commands[i]);
	return WM_SUCCESS;
}

int mfg_psm_init()
{
	if (mfg_psm_init_done)
		return WM_SUCCESS;
	int ret = part_init();
	if (ret != WM_SUCCESS)
		return -WM_FAIL;
#ifdef __linux__
	mfg_psm_init_done = true;
	return WM_SUCCESS;
#else /* ! __linux__ */
	flash_desc_t fl;
	struct partition_entry *p;
	psm_cfg_t psm_cfg;
	memset(&psm_cfg, 0, sizeof(psm_cfg_t));
#ifdef CONFIG_SECURE_PSM
	psm_cfg.secure = 1;
#else /* !CONFIG_SECURE_PSM */
	psm_cfg.secure = 0;
#endif /* CONFIG_SECURE_PSM */

	p = part_get_layout_by_name("mfg", NULL);
	if (p == NULL)
		return -WM_FAIL;
	part_to_flash_desc(p, &fl);
	ret = psm_module_init(&fl, &mfg_psm_hnd, &psm_cfg);

	if (ret == -WM_E_PERM) {
		mfg_d("Initializing MFG partition as Read-Only");
		psm_cfg.read_only = true;
		ret = psm_module_init(&fl, &mfg_psm_hnd,
				      &psm_cfg);
	}

	if (ret == WM_SUCCESS)
		mfg_psm_init_done = true;
	return ret;
#endif /* __linux__ */
}
