/*
 * Copyright (C) 2008-2014, Marvell International Ltd.
 * All Rights Reserved.
 */

#ifndef _WMCLOUD_LP_WS_H_
#define _WMCLOUD_LP_WS_H_

#include <wmcloud.h>
/* Cloud's Exported Functions */

int cloud_start(const char *dev_class, void (*handle_req)(struct json_str
		*jstr, jobj_t *obj, bool *repeat_POST),
		void (*periodic_post)(struct json_str *jstr));
int cloud_stop(void);
#endif
