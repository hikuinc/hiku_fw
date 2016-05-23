/*  Copyright 2008-2014, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef __NTPC_H__
#define __NTPC_H__

/** NTP synchronize. The Posix time is set automatically. Call this
    function only when network access is present */
int ntpc_sync(const char *ntp_server, uint32_t max_num_pkt_xchange);

#endif /* __NTPC_H__ */
