/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <wmstdio.h>
#include <wm_os.h>
#include <json_parser.h>
#include <json_generator.h>
#include <board.h>
#include <wmcloud.h>
#include <appln_dbg.h>
#include <led_indicator.h>

#include <wmcloud_lp_ws.h>
#define J_NAME_DATA	"data"
#define J_NAME_AIO	"aio"
#define J_NAME_STATE          "led_state"

#if APPCONFIG_DEMO_CLOUD
#include "aio_cloud.h"
#define DEVICE_CLASS	"aio"
#endif  /* APPCONFIG_DEMO_CLOUD */

void aio_periodic_post(struct json_str *jstr)
{
	json_push_object(jstr, J_NAME_AIO);
	json_set_val_int(jstr, J_NAME_STATE, aio_get_led_state());
	json_pop_object(jstr);
}

void
aio_handle_req(struct json_str *jstr, jobj_t *obj, bool *repeat_POST)
{

	char buf[16];
	bool flag = false;
	int req_state;
	if (json_get_composite_object(obj, J_NAME_AIO)
			== WM_SUCCESS) {
		if (json_get_val_str(obj, J_NAME_STATE, buf, 16)
		    == WM_SUCCESS) {
			if (!strncmp(buf, QUERY_STR, 16)) {
				dbg("Led state query");
				*repeat_POST = flag = true;
			}
		} else if (json_get_val_int(obj, J_NAME_STATE,
				 &req_state) == WM_SUCCESS) {
			if (req_state == 0) {
				aio_led_off();
				*repeat_POST = flag = true;
			} else if (req_state == 1) {
				aio_led_on();
				*repeat_POST = flag = true;
			}
			dbg("Led state set to %d", aio_get_led_state());
		}

		if (flag) {
			json_push_object(jstr, J_NAME_AIO);
			json_set_val_int(jstr, J_NAME_STATE,
					 aio_get_led_state());
			json_pop_object(jstr);
		}
		json_release_composite_object(obj);
	}
}

#if APPCONFIG_DEMO_CLOUD
void aio_cloud_start()
{
	int ret;
	/* Starting cloud thread if enabled */
	ret = cloud_start(DEVICE_CLASS, aio_handle_req,
		aio_periodic_post);
	if (ret != WM_SUCCESS)
		dbg("Unable to start the cloud service");
}
void aio_cloud_stop()
{
	int ret;
	/* Stop cloud server */
	ret = cloud_stop();
	if (ret != WM_SUCCESS)
		dbg("Unable to stop the cloud service");
}
#else
void aio_cloud_start() {}
void aio_cloud_stop() {}
#endif /* APPCONFIG_DEMO_CLOUD */
