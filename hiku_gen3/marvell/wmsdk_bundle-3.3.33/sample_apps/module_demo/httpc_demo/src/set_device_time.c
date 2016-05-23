
/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* Setting device time */

#include <openssl/ssl.h>
#include <wmtypes.h>
#include <wmstdio.h>
#include <wmtime.h>
#include <string.h>
#include <wmstdio.h>
#include <wm_os.h>
#include <wm_net.h>
#include <httpc.h>
#include <cli.h>
#include <cli_utils.h>
#include <cyassl/ctaocrypt/settings.h>
#include <appln_dbg.h>
#include <wlan.h>
#include <wm-tls.h>
#include <mdev_aes.h>
#include <json_parser.h>
#include <httpc.h>
#include <wmstdio.h>
#define MAX_DOWNLOAD_DATA 1024
#define SET_TIME_URL "http://api.evrythng.com/time"

/* Function to set device time (called from main)*/
void set_device_time()
{
	http_session_t handle;
	http_resp_t *resp = NULL;
	char buf[MAX_DOWNLOAD_DATA];
	int64_t timestamp, offset;
	int size = 0;
	int status = 0;


	wmprintf("Get time from : %s\r\n", SET_TIME_URL);
	status = httpc_get(SET_TIME_URL, &handle, &resp, NULL);
	if (status != WM_SUCCESS) {
		wmprintf("Getting URL failed");
		return;
	}
	size = http_read_content(handle, buf, MAX_DOWNLOAD_DATA);
	if (size <= 0) {
		wmprintf("Reading time failed\r\n");
		goto out_time;
	}
	/*
	  If timezone is present in PSM
	  Get on http://api.evrythng.com/time?tz=<timezone>
	  The data will look like this
	  {
	  "timestamp":1429514751927,
	  "offset":-18000000,
	  "localTime":"2015-04-20T02:25:51.927-05:00",
	  "nextChange":1446361200000
	  }
	  If timezone is not presentin PSM
	  Get on http://api.evrythng.com/time
	  The data will look like this
	  {
	  "timestamp":1429514751927
	  }
	*/
	jsontok_t json_tokens[9];
	jobj_t json_obj;
	if (json_init(&json_obj, json_tokens, 10, buf, size) != WM_SUCCESS) {
		wmprintf("Wrong json string\r\n");
		goto out_time;
	}

	if (json_get_val_int64(&json_obj, "timestamp", &timestamp)
	    == WM_SUCCESS) {
		if (json_get_val_int64(&json_obj, "offset", &offset)
		    != WM_SUCCESS) {
			offset = 0;
		}
		timestamp = timestamp + offset;
	}
	wmtime_time_set_posix(timestamp/1000);
 out_time:
	http_close_session(&handle);
	return;
}
