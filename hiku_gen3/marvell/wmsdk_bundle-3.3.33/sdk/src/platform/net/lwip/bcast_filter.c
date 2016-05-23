/*
 * Copyright (C) 2008-2015, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <wm_net.h>
#include <lwip/tcpip.h>
#include "netif/etharp.h"

#define UDP_PACKET 0x11

#define NETIF_STATS_INC(num) (++num)
#define NETIF_STATS_DEC(num) (--num)

#define PLATFORM_DIAG(_x_) do { unsigned int ts = os_get_timestamp(); \
wmprintf("[%d.%06d] : ", ts/1000000, ts%1000000); \
wmprintf _x_ ; \
wmprintf("\r"); \
} while (0);

struct stats_udp_bcast {
	uint32_t recv;             /* Received packets. */
	uint32_t fw;               /* Forwarded packets. */
	uint32_t drop;             /* Dropped packets. */
};

static struct stats_udp_bcast udp_bcast;

struct udp_filter {
	uint16_t port;
	struct udp_filter *next_filter;
};

typedef struct udp_filter udp_bcast_filter_t;
static udp_bcast_filter_t *udp_bcast_filter;

int netif_add_udp_broadcast_filter(uint16_t port_number)
{
	udp_bcast_filter_t *trace_filter = udp_bcast_filter;
	while (trace_filter != NULL) {
		if (trace_filter->port == port_number)
			return WM_SUCCESS;
		trace_filter = trace_filter->next_filter;
	}

	udp_bcast_filter_t *bcast_filter = os_mem_alloc(
		sizeof(udp_bcast_filter_t));
	if (bcast_filter == NULL) {
		LWIP_DEBUGF(NETIF_DEBUG, ("UDP broadcast filter: "
					  "out of memory\n"));
		return -WM_FAIL;
	}
	bcast_filter->port = port_number;
	bcast_filter->next_filter = NULL;

	if (udp_bcast_filter == NULL) {
		udp_bcast_filter = bcast_filter;
		return WM_SUCCESS;
	}

	trace_filter = udp_bcast_filter;
	while (trace_filter->next_filter != NULL) {
		trace_filter = trace_filter->next_filter;
	}
	trace_filter->next_filter = bcast_filter;
	return WM_SUCCESS;
}

int netif_remove_udp_broadcast_filter(uint16_t port_number)
{
	if (udp_bcast_filter == NULL)
		return -WM_FAIL;

	udp_bcast_filter_t *bcast_filter = udp_bcast_filter;

	if (bcast_filter->port == port_number) {
		udp_bcast_filter = bcast_filter->next_filter;
		bcast_filter->next_filter = NULL;
		os_mem_free(bcast_filter);
		return WM_SUCCESS;
	}
	while (bcast_filter->next_filter &&
	       bcast_filter->next_filter->port != port_number) {
		bcast_filter = bcast_filter->next_filter;
	}
	if (bcast_filter->next_filter) {
		udp_bcast_filter_t *temp;
		temp = bcast_filter->next_filter;
		bcast_filter->next_filter =
			temp->next_filter;
		temp->next_filter = NULL;
		os_mem_free(temp);
		return WM_SUCCESS;
	}
	return -WM_FAIL;
}

bool netif_unwanted_udp_broadcast_packet(struct pbuf *p)
{
	struct eth_hdr *ethhdr = p->payload;
	struct ip_hdr *iphdr = p->payload + SIZEOF_ETH_HDR;
	if (iphdr->_proto == UDP_PACKET) {
		uint8_t broadcast_addr[] = {
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
		if (!memcmp(ethhdr->dest.addr, broadcast_addr,
			    ETHARP_HWADDR_LEN)) {
			NETIF_STATS_INC(udp_bcast.recv);
			struct udp_hdr *udphdr = p->payload + SIZEOF_ETH_HDR +
				sizeof(struct ip_hdr);
			udp_bcast_filter_t *bcast_filter =
				udp_bcast_filter;
			while (bcast_filter != NULL) {
				if (ntohs(udphdr->dest) ==
				    bcast_filter->port) {
					NETIF_STATS_INC(udp_bcast.fw);
					break;
				}
				bcast_filter =
					bcast_filter->next_filter;
				if (bcast_filter == NULL) {
					pbuf_free(p);
					NETIF_STATS_INC(udp_bcast.drop);
					LINK_STATS_INC(link.drop);
					LINK_STATS_INC(udp.drop);
					return true;
				}
			}
		}
	}
	return false;
}

void stats_udp_bcast_display()
{
	PLATFORM_DIAG(("\nUDP BCAST\n\t"));
	PLATFORM_DIAG(("recv: %u\n\t", udp_bcast.recv));
	PLATFORM_DIAG(("accept: %u\n\t", udp_bcast.fw));
	PLATFORM_DIAG(("drop: %u\n\t", udp_bcast.drop));
}
