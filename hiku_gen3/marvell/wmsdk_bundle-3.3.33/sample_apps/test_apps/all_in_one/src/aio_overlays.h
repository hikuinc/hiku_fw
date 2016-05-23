/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */


#ifndef __AIO_OVERLAYS_H__
#define __AIO_OVERLAYS_H__

#if APPCONFIG_OVERLAY_ENABLE
void aio_load_cloud_overlay();
void aio_load_wps_overlay();

#else

static inline void aio_load_cloud_overlay() {}
static inline void aio_load_wps_overlay() {}

#endif   /* APPCONFIG_OVERLAY_ENABLE */

#endif   /* __AIO_OVERLAYS_H__ */

