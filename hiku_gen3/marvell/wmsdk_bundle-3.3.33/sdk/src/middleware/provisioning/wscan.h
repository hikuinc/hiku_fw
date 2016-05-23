/*
 * Copyright 2008-2015, Marvell International Ltd.
 * All Rights Reserved.
 *
 */

#ifndef __WSCAN_H__
#define __WSCAN_H__

/* wscan_periodic_start() and wscan_periodic_stop() enqueue and dequeue
 * wlan_scan() to periodically scan for networks in the
 * vicinity. wscan_get_scan_results() and wscan_get_scan_count() return scan
 * results.
 * To access scan results, application needs to get the survey lock first and
 * then access the scan results. Once application is done accessing the scan
 * results it should release the lock.
 */
int wscan_get_scan_result(struct wlan_scan_result **sptr, unsigned int index);
int wscan_get_scan_count();
void wscan_put_lock_scan_results();
int wscan_get_lock_scan_results();

#endif /* __WSCAN_H__ */
