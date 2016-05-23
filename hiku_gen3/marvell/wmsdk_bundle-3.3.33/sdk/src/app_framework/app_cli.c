/*
 *  Copyright (C) 2008-2016 Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wm_os.h>
#include <psm-v2.h>
#include <cli.h>

#include "app_cli.h"

extern psm_hnd_t app_psm_hnd;

static int _cli_name_val_get(const char *name, char *value, int max_len)
{
	return psm_get_variable_str(app_psm_hnd, name, value, max_len);
}

static int _cli_name_val_set(const char *name, const char *value)
{
	return psm_set_variable_str(app_psm_hnd, name, value);
}

int app_psm_cli_hist_init()
{
	if (!app_psm_hnd)
		return -WM_FAIL;

	return cli_add_history_hook(_cli_name_val_get, _cli_name_val_set);
}
