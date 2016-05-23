/*
 *  Copyright (C) 2015, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <wmstdio.h>
#include <mdns.h>
#include "mdns_private.h"

extern mdns_responder_stats mr_stats;

int mdns_stat()
{
	wmprintf("mDNS Stats\r\n");
	wmprintf("=== Rx Stats\r\n");
	wmprintf("Total \t= %10d\r\n", mr_stats.total_rx);
	wmprintf("Queries = %10d \tAnswers \t= %10d\r\n", \
		mr_stats.rx_queries, mr_stats.rx_answers);
	wmprintf("Errors  = %10d \tKnown Answers\t= %10d\r\n", \
		mr_stats.rx_errors, mr_stats.rx_known_ans);
	wmprintf("====== Conflicts \r\nHost name \t= %10u "
		"\tService name \t= %10u\r\n", \
		mr_stats.rx_hn_conflicts,
		mr_stats.rx_sn_conflicts);

	wmprintf("=== Tx Stats\r\n");
	wmprintf("Total \t= %10d\r\n", mr_stats.total_tx);
	wmprintf("Probes \t= %10d \tAnnouncements \t= %10d " \
		"\tClosing probes \t= %10d\r\n", \
		mr_stats.tx_probes, mr_stats.tx_announce, \
		mr_stats.tx_bye);
	wmprintf("Response= %10d \tReannounce \t= %10d\r\n", \
		mr_stats.tx_response, mr_stats.tx_reannounce);
	wmprintf("====== Errors \r\nIPv4 \t= %10d "
		"\tIPv6 \t\t= %10d\r\n", \
		mr_stats.tx_ipv4_err, mr_stats.tx_ipv6_err);
	wmprintf("====== Current stats \r\nProbes \t= %10u "
		"\tAnnouncements \t= %10u\tRx events \t= %10u\r\n", \
		mr_stats.tx_probes_curr, mr_stats.tx_announce_curr, \
		mr_stats.probe_rx_events_curr);
	wmprintf("====== Interface stats\r\n");
	wmprintf("UP \t= %10u \tDOWN \t= %10u \tREANNOUNCE \t= %10u\r\n", \
			mr_stats.iface_up, mr_stats.iface_down,
			mr_stats.iface_reannounce);
	return 0;
}

