/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wmstdio.h>
#include <overlays.h>

/* first overlay part is WPS */
#define OVERLAY_PART_WPS   0
/* second overlay part is cloud */
#define OVERLAY_PART_CLOUD 1

extern struct overlay ovl;


void aio_load_cloud_overlay()
{
	overlay_load(&ovl, OVERLAY_PART_CLOUD);
}

void aio_load_wps_overlay()
{
	overlay_load(&ovl, OVERLAY_PART_WPS);
}
