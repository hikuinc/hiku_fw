/**
 * @file
 * Ethernet Interface Skeleton
 *
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

/*
 * This file is a skeleton for developing Ethernet network interface
 * drivers for lwIP. Add code to the low_level functions and do a
 * search-and-replace for the word "ethernetif" to replace it with
 * something that better describes your network interface.
 */

#include <mlan_wmsdk.h>

#include <wm_os.h>
#include <wm_net.h>
#include <wmassert.h>
#include <pwrmgr.h>
#include <wmlog.h>

#include "lwip/opt.h"

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/sys.h"
#ifdef CONFIG_IPV6
#include "lwip/ethip6.h"
#endif /* CONFIG_IPV6 */
#include <lwip/stats.h>
#include <lwip/snmp.h>
#include "netif/etharp.h"
#include "netif/ppp_oe.h"

#include <wifi-sdio.h>
#include <wifi-internal.h>

#include <lowlevel_drivers.h>

#ifdef CONFIG_HOST_SUPP
#include <wm_supplicant.h>
#endif
/* Define those to better describe your network interface. */
#define IFNAME0 'm'
#define IFNAME1 'l'

/*
 * It was observed that Wi-Fi card does not wakeup
 * immediately after call to wlan_wakeup_card.
 * The code tries to wakeup the card by writing
 * in SDIO register.
 * It waits for 20 ms for AWAKE event from Wi-Fi card,
 * if AWAKE event is not generated or received  by MCU code
 * it returns error and does not send a packet out.
 * This is observed with  8801 Wi-Fi card.
 * So for 8801 based platforms the wait time is now 35 ms.
 */

#ifdef CONFIG_WiFi_878x
#define MAX_WAIT_TIME 20
#else
#define MAX_WAIT_TIME 35
#endif

extern int wlan_get_mac_address(t_u8 *);
extern void wlan_wake_up_card();
extern bool netif_unwanted_udp_broadcast_packet(struct pbuf *);

int raw_process_pkt_hdrs(void *pbuf, t_u32 payloadlen, t_u8 interface);
mlan_status wlan_xmit_pkt(uint32_t txlen, uint8_t interface);

#ifdef CONFIG_P2P
mlan_status wlan_send_gen_sdio_cmd(uint8_t *buf, uint32_t buflen);
#endif

uint16_t g_data_nf_last;
uint16_t g_data_snr_last;

#define MAX_INTERFACES_SUPPORTED 3
static struct netif *netif_arr[MAX_INTERFACES_SUPPORTED];
static t_u8 rfc1042_eth_hdr[MLAN_MAC_ADDR_LENGTH] = {
	0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };

#ifdef CONFIG_P2P
extern int wlan_get_wfd_mac_address(t_u8 *);
extern int wfd_bss_type;
#endif

#ifdef CONFIG_WPS2
void (*wps_rx_callback)(const t_u8 *buf, size_t len);
#endif

#ifdef CONFIG_HOST_SUPP
void (*supplicant_rx_callback)(const t_u8 interface,
			const t_u8 *buf, size_t len);
#endif /* CONFIG_HOST_SUPP */

void (*sniffer_callback)(const wifi_frame_t *frame, const uint16_t len);

/**
 * Helper struct to hold private data used to operate your ethernet interface.
 * Keeping the ethernet address of the MAC in this struct is not necessary
 * as it is already kept in the struct netif.
 * But this is only an example, anyway...
 */
struct ethernetif {
	struct eth_addr *ethaddr;
	/* Interface to bss type identification that tells the FW wherether
	   the data is for STA for UAP */
	t_u8  interface;
	/* Add whatever per-interface state that is needed here. */
};

/* The time to block waiting for input. */
#define emacBLOCK_TIME_WAITING_FOR_INPUT	((portTickType) 100)

int wrapper_wlan_handle_rx_packet(t_u16 datalen, RxPD *rxpd, struct pbuf *p);
int wrapper_wlan_handle_amsdu_rx_packet(const t_u8 *rcvdata,
					const t_u16 datalen);

static void register_interface(struct netif *iface,
				   mlan_bss_type iface_type)
{
	ASSERT(iface_type < MAX_INTERFACES_SUPPORTED);
	netif_arr[iface_type] = iface;
}

void deliver_packet_above(struct pbuf *p, int recv_interface)
{
	err_t lwiperr = ERR_OK;
	/* points to packet payload, which starts with an Ethernet header */
	struct eth_hdr *ethhdr = p->payload;

	switch (htons(ethhdr->type)) {
	case ETHTYPE_IP:
#ifdef CONFIG_IPV6
	case ETHTYPE_IPV6:
#endif
	case ETHTYPE_ARP:
		if (recv_interface >= MAX_INTERFACES_SUPPORTED) {
			wmlog("lwip", "Error:, Interface index: %d (0x%x) "
				 "not registered R: %p\n\r", recv_interface,
				 recv_interface, __builtin_return_address(0));
			wmpanic();
		}

		/* full packet send to tcpip_thread to process */
		lwiperr = netif_arr[recv_interface]->input(p,
							 netif_arr[recv_interface]);
		if (lwiperr != ERR_OK) {
			LINK_STATS_INC(link.proterr);
			LWIP_DEBUGF(NETIF_DEBUG,
				    ("ethernetif_input: IP input error\n"));
			pbuf_free(p);
			p = NULL;
		}
		break;
	case ETHTYPE_EAPOL:
#ifdef CONFIG_WPS2
		if (wps_rx_callback)
			wps_rx_callback(p->payload, p->len);
#endif /* CONFIG_WPS2 */

#ifdef CONFIG_HOST_SUPP
		if (supplicant_rx_callback)
			supplicant_rx_callback(recv_interface,
					p->payload, p->len);
#endif /* CONFIG_HOST_SUPP */
		pbuf_free(p);
		p = NULL;
		break;
	default:
		/* drop the packet */
		LINK_STATS_INC(link.drop);
		pbuf_free(p);
		p = NULL;
		break;
	}
}

struct pbuf *gen_pbuf_from_data(t_u8 *payload, t_u16 datalen)
{
	/* We allocate a pbuf chain of pbufs from the pool. */
	struct pbuf *p = pbuf_alloc(PBUF_RAW, datalen, PBUF_POOL);
	if (!p)
		return NULL;

	/* Generate a linked list of pbuf's depending on the size of the packet */
	struct pbuf *q;
	int pkt_len = 0;
	for (q = p; q != NULL; q = q->next) {
		/* fixme: Check if whole q->len needs to be copied b'coz
		   rxpd->rx_pkt_length is less than that*/
		memcpy(q->payload, payload + pkt_len, q->len);
		pkt_len += q->len;
		/* fixme: check if tot_len field in pbuf also needs to be
		   updated */
	}

	return p;
}

static void process_data_packet(const t_u8 *rcvdata, const t_u16 datalen)
{
	RxPD *rxpd = (RxPD *) ((t_u8 *) rcvdata + INTF_HEADER_LEN);
	int recv_interface = rxpd->bss_type;

	if (rxpd->rx_pkt_type == PKT_TYPE_AMSDU) {
#ifdef CONFIG_11N
		wrapper_wlan_handle_amsdu_rx_packet(rcvdata, datalen);
#endif /* CONFIG_11N */
		return;
	}

	if (recv_interface == MLAN_BSS_TYPE_STA) {
		g_data_nf_last = rxpd->nf;
		g_data_snr_last = rxpd->snr;
	}

	t_u8 *payload = (t_u8 *) rxpd + rxpd->rx_pkt_offset;
	struct pbuf *p = gen_pbuf_from_data(payload, datalen);
	/* If there are no more buffers, we do nothing, so the data is 
	   lost. We have to go back and read the other ports */
	if (p == NULL) {
		LINK_STATS_INC(link.memerr);
		LINK_STATS_INC(link.drop);
		return;
	}

	/* points to packet payload, which starts with an Ethernet header */
	struct eth_hdr *ethhdr = p->payload;

	if (!memcmp(p->payload + SIZEOF_ETH_HDR, rfc1042_eth_hdr,
		    sizeof(rfc1042_eth_hdr))) {
		struct eth_llc_hdr *ethllchdr = p->payload + SIZEOF_ETH_HDR;
		memcpy(&ethhdr->type, (const void *)&ethllchdr->type,
		       sizeof(u16_t));
		p->len -= SIZEOF_ETH_LLC_HDR;
		memcpy(p->payload + SIZEOF_ETH_HDR,
		       p->payload + SIZEOF_ETH_HDR + SIZEOF_ETH_LLC_HDR,
		       p->len - SIZEOF_ETH_LLC_HDR);
	}
	switch (htons(ethhdr->type)) {
	case ETHTYPE_IP:
#ifdef CONFIG_IPV6
	case ETHTYPE_IPV6:
#endif
		/* To avoid processing of unwanted udp broadcast packets, adding
		 * filter for dropping packets received on ports other than
		 * pre-defined ports.
		 */
		if (netif_unwanted_udp_broadcast_packet(p)) {
			break;
		}
		LINK_STATS_INC(link.recv);
#ifdef CONFIG_11N
		if (recv_interface == MLAN_BSS_TYPE_STA) {
			int rv = wrapper_wlan_handle_rx_packet(datalen,
							       rxpd, p);
			if (rv != WM_SUCCESS) {
				/* mlan was unsuccessful in delivering the
				   packet */
				LINK_STATS_INC(link.drop);
				pbuf_free(p);
			}
		}
		else
			deliver_packet_above(p, recv_interface);
#else /* ! CONFIG_11N */
		deliver_packet_above(p, recv_interface);
#endif /* CONFIG_11N */
		p = NULL;
		break;
	case ETHTYPE_ARP:
	case ETHTYPE_EAPOL:
		LINK_STATS_INC(link.recv);
		deliver_packet_above(p, recv_interface);
		break;
	default:

		if (sniffer_callback) {
			wifi_frame_t *frame = (wifi_frame_t *)
				(uint8_t *) p->payload;

			if (frame->frame_type == BEACON_FRAME ||
				frame->frame_type == DATA_FRAME ||
				frame->frame_type == AUTH_FRAME ||
			    frame->frame_type == PROBE_REQ_FRAME ||
				frame->frame_type == QOS_DATA_FRAME)
				sniffer_callback(frame, rxpd->rx_pkt_length);
		}

		/* fixme: avoid pbuf allocation in this case */
		LINK_STATS_INC(link.drop);
		pbuf_free(p);
		break;							
	}
}

/* Callback function called from the wifi module */
void handle_data_packet(const t_u8 interface, const t_u8 *rcvdata,
			const t_u16 datalen)
{
	if (netif_arr[interface])
		process_data_packet(rcvdata, datalen);
}

/**
 * Should be called at the beginning of the program to set up the
 * In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
void low_level_init(struct netif *netif)
{
	/* set MAC hardware address length */
	netif->hwaddr_len = ETHARP_HWADDR_LEN;

	/* set MAC hardware address */
#ifndef STANDALONE
	wlan_get_mac_address(netif->hwaddr);
#endif

	/* maximum transfer unit */
	netif->mtu = 1500;

	/* device capabilities */
	/* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
	netif->flags =
	    NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP |
	    NETIF_FLAG_IGMP;
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become availale since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
	int ret;
	struct pbuf *q;
	struct ethernetif *ethernetif = netif->state;
	u32_t pkt_len;

	memset(outbuf, 0x00, sizeof(outbuf));

	pkt_len = sizeof(TxPD) + INTF_HEADER_LEN;

	for (q = p; q != NULL; q = q->next) {
		if (pkt_len > sizeof(outbuf)) {
			while (1) {
				LWIP_DEBUGF(NETIF_DEBUG, ("PANIC: Xmit packet"
					"is bigger than inbuf.\r\n"));
				vTaskDelay((3000) / portTICK_RATE_MS);
			}
		}
		memcpy((u8_t *) outbuf + pkt_len, (u8_t *) q->payload, q->len);
		pkt_len += q->len;
	}

	ret = wifi_low_level_output(ethernetif->interface, outbuf +
		sizeof(TxPD) + INTF_HEADER_LEN, pkt_len -
		sizeof(TxPD) - INTF_HEADER_LEN);

	if (ret == -WM_E_NOMEM) {
		LINK_STATS_INC(link.err);
		ret = ERR_MEM;
	} else if (ret == -WM_E_BUSY) {
		LINK_STATS_INC(link.err);
		ret = ERR_TIMEOUT;
	} else if (ret == WM_SUCCESS) {
		LINK_STATS_INC(link.xmit);
		ret = ERR_OK;
	}

	return ret;
}

int raw_low_level_output(const u8_t interface, const u8_t *buf, t_u32 len)
{
	int i;
	u32_t pkt_len;

	wifi_sdio_lock();

	/* XXX: TODO Get rid on the memset once we are convinced that
	 * process_pkt_hdrs sets correct values */
	memset(outbuf, 0, sizeof(outbuf));

	pkt_len = raw_process_pkt_hdrs((t_u8 *) outbuf, 0, interface);
	memcpy((u8_t *) outbuf + pkt_len, buf, len);

	i = wlan_xmit_pkt(pkt_len + len, interface);
	if (i == MLAN_STATUS_FAILURE) {
		LINK_STATS_INC(link.err);
		wifi_sdio_unlock();
		return ERR_MEM;
	}
	LINK_STATS_INC(link.xmit);
	wifi_sdio_unlock();

	return WM_SUCCESS;
}

#ifdef CONFIG_WPS2
int wps_low_level_output(const u8_t interface, const u8_t *buf, t_u32 len)
{
	int i;
	u32_t pkt_len;

	if (len > sizeof(outbuf)) {
		while (1) {
			LWIP_DEBUGF(NETIF_DEBUG, ("PANIC: Xmit packet"
						"is bigger than inbuf.\r\n"));
			vTaskDelay((3000) / portTICK_RATE_MS);
		}
	}

	wifi_sdio_lock();

	/* XXX: TODO Get rid on the memset once we are convinced that
	 * process_pkt_hdrs sets correct values */
	memset(outbuf, 0, sizeof(outbuf));

	pkt_len = sizeof(TxPD) + INTF_HEADER_LEN;

	memcpy((u8_t *) outbuf + pkt_len, buf, len);

	i = wlan_xmit_pkt(pkt_len + len, interface);

	if (i == MLAN_STATUS_FAILURE) {
		LINK_STATS_INC(link.err);
		wifi_sdio_unlock();
		return ERR_MEM;
	}
	LINK_STATS_INC(link.xmit);
	wifi_sdio_unlock();
	return ERR_OK;
}

void wps_register_rx_callback(void (*WPSEAPoLRxDataHandler)
				(const t_u8 *buf, const size_t len))
{
	wps_rx_callback = WPSEAPoLRxDataHandler;
}

void wps_deregister_rx_callback()
{
	wps_rx_callback = NULL;
}
#endif

#ifdef CONFIG_HOST_SUPP
int supp_low_level_output(const u8_t interface, const u8_t *buf, t_u32 len)
{
	int i;
	u32_t pkt_len;

	if (len > sizeof(outbuf)) {
		while (1) {
			LWIP_DEBUGF(NETIF_DEBUG, ("PANIC: Xmit packet"
						"is bigger than inbuf.\r\n"));
			vTaskDelay((3000) / portTICK_RATE_MS);
		}
	}

	wifi_sdio_lock();

	/* XXX: TODO Get rid on the memset once we are convinced that
	 * process_pkt_hdrs sets correct values */
	memset(outbuf, 0, sizeof(outbuf));

	pkt_len = sizeof(TxPD) + INTF_HEADER_LEN;

	memcpy((u8_t *) outbuf + pkt_len, buf, len);

	i = wlan_xmit_pkt(pkt_len + len, interface);

	if (i == MLAN_STATUS_FAILURE) {
		LINK_STATS_INC(link.err);
		wifi_sdio_unlock();
		return ERR_MEM;
	}
	LINK_STATS_INC(link.xmit);
	wifi_sdio_unlock();
	return ERR_OK;
}

void supplicant_register_rx_callback(void (*EAPoLRxDataHandler)
				(const t_u8 interface,
				const t_u8 *buf, const size_t len))
{
	supplicant_rx_callback = EAPoLRxDataHandler;
}

void supplicant_deregister_rx_callback()
{
	supplicant_rx_callback = NULL;
}
#endif /* CONFIG_HOST_SUPP */

void sniffer_register_callback(void (*sniffer_cb)
				(const wifi_frame_t *frame,
				 const uint16_t len))
{
	sniffer_callback = sniffer_cb;
}

void sniffer_deregister_callback()
{
	sniffer_callback = NULL;
}

#ifdef CONFIG_P2P
int netif_get_bss_type()
{
	return wfd_bss_type;
}
#endif

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netifapi_netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t lwip_netif_init(struct netif *netif)
{
	struct ethernetif *ethernetif;

	LWIP_ASSERT("netif != NULL", (netif != NULL));

	ethernetif = mem_malloc(sizeof(struct ethernetif));
	if (ethernetif == NULL) {
		LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_init: out of memory\n"));
		return ERR_MEM;
	}

	/*
	 * Initialize the snmp variables and counters inside the struct netif.
	 * The last argument should be replaced with your link speed, in units
	 * of bits per second.
	 */
	NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd,
			LINK_SPEED_OF_YOUR_NETIF_IN_BPS);

	ethernetif->interface = MLAN_BSS_TYPE_STA;
	netif->state = ethernetif;
	netif->name[0] = IFNAME0;
	netif->name[1] = IFNAME1;
	/* We directly use etharp_output() here to save a function call.
	 * You can instead declare your own function an call etharp_output()
	 * from it if you have to do some checks before sending (e.g. if link
	 * is available...) */
	netif->output = etharp_output;
	netif->linkoutput = low_level_output;
#ifdef CONFIG_IPV6
	netif->output_ip6 = ethip6_output;
#endif

	ethernetif->ethaddr = (struct eth_addr *)&(netif->hwaddr[0]);

	/* initialize the hardware */
	low_level_init(netif);
	register_interface(netif, MLAN_BSS_TYPE_STA);
	return ERR_OK;
}

err_t lwip_netif_uap_init(struct netif *netif)
{
	struct ethernetif *ethernetif;

	LWIP_ASSERT("netif != NULL", (netif != NULL));

	ethernetif = mem_malloc(sizeof(struct ethernetif));
	if (ethernetif == NULL) {
		LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_init: out of memory\n"));
		return ERR_MEM;
	}

	ethernetif->interface = MLAN_BSS_TYPE_UAP;
	netif->state = ethernetif;
	netif->name[0] = 'u';
	netif->name[1] = 'a';
	/* We directly use etharp_output() here to save a function call.
	 * You can instead declare your own function an call etharp_output()
	 * from it if you have to do some checks before sending (e.g. if link
	 * is available...) */
	netif->output = etharp_output;
	netif->linkoutput = low_level_output;

	ethernetif->ethaddr = (struct eth_addr *)&(netif->hwaddr[0]);

	/* initialize the hardware */
	low_level_init(netif);
	register_interface(netif, MLAN_BSS_TYPE_UAP);

	return ERR_OK;
}

#ifdef CONFIG_P2P
err_t lwip_netif_wfd_init(struct netif *netif)
{
	struct ethernetif *ethernetif;

	LWIP_ASSERT("netif != NULL", (netif != NULL));

	ethernetif = mem_malloc(sizeof(struct ethernetif));
	if (ethernetif == NULL) {
		LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_init: out of memory\n"));
		return ERR_MEM;
	}

	ethernetif->interface = MLAN_BSS_TYPE_WIFIDIRECT;
	netif->state = ethernetif;
	netif->name[0] = 'w';
	netif->name[1] = 'f';
	/* We directly use etharp_output() here to save a function call.
	 * You can instead declare your own function an call etharp_output()
	 * from it if you have to do some checks before sending (e.g. if link
	 * is available...) */
	netif->output = etharp_output;
	netif->linkoutput = low_level_output;

	ethernetif->ethaddr = (struct eth_addr *)&(netif->hwaddr[0]);

	/* initialize the hardware */
	low_level_init(netif);
#ifndef STANDALONE
	wlan_get_wfd_mac_address(netif->hwaddr);
#endif
	register_interface(netif, MLAN_BSS_TYPE_WIFIDIRECT);
	return ERR_OK;
}
#endif
