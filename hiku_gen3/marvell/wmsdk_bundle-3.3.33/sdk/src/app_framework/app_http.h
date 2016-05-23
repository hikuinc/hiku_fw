/*
 *  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */
#ifndef _APP_HTTP_H_
#define _APP_HTTP_H_

#include <json_parser.h>
#include <wm_os.h>
#include <app_framework.h>

int app_sys_normal_set_network_cb();
int app_sys_register_file_handler();
int app_sys_register_sys_handler();


/** Tick at which last http access was recorded */
unsigned long app_http_get_last_access_time();

/** Retrieve the system specific information in JSON format */
void app_sys_http_get_info(struct json_str *jptr);

int app_sys_http_update_all(jobj_t *obj, short *fs_done,
			    short *fw_done, short *wififw_done);

int sys_handle_webcmd(jobj_t *obj);

int sys_handle_get_watchdog(struct json_str *jptr);

int sys_handle_set_watchdog(jobj_t *obj);

static inline int sys_handle_get_diag_stats_live(struct json_str *jptr)
{
	return diagnostics_read_stats(jptr, app_psm_hnd);
}

static inline int sys_handle_get_diag_stats_history(struct json_str *jptr)
{
	return diagnostics_read_stats_psm(jptr, app_psm_hnd);
}

#endif
