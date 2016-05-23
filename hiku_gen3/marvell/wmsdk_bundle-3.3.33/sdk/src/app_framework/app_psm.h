/*
 *  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _APP_PSM_H
#define _APP_PSM_H

#define SYS_MOD_NAME		"sys"
#define NETWORK_MOD_NAME        "network"

#define VAR_SYS_DEVICE_NAME	"sys.name"
#define VAR_SYS_DEVICE_ALIAS	"sys.alias"


/** Init function for applications psm module */
int app_psm_init(void);

/** Initialize Device alias in psm */
int app_init_device_alias(char *alias);

/** Get current device alias */
int app_get_device_alias(char *alias, int len);

/** Set device alias */
int app_set_device_alias(char *alias);

/** Handle psm partition error in case of corruption */
void app_psm_handle_partition_error(int errno, const char *module_name);

#endif /* ! _APP_PSM_H */
