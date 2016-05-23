/*
 * Copyright (C) 2008-2015, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <inttypes.h>
#include <wmstdio.h>
#include <wifi.h>
#include <wm_os.h>
#include <wm_net.h>
#include <wlan.h>
#include <lwip/netifapi.h>
#include <lwip/tcpip.h>
#include <lwip/dns.h>
#include <lwip/dhcp.h>
#include <wmlog.h>

#ifdef CONFIG_IPV6
#define IPV6_ADDR_STATE_TENTATIVE       "Tentative"
#define IPV6_ADDR_STATE_PREFERRED       "Preferred"
#define IPV6_ADDR_STATE_INVALID         "Invalid"
#define IPV6_ADDR_STATE_VALID           "Valid"
#define IPV6_ADDR_STATE_DEPRECATED      "Deprecated"
#define IPV6_ADDR_TYPE_LINKLOCAL	"Link-Local"
#define IPV6_ADDR_TYPE_GLOBAL		"Global"
#define IPV6_ADDR_TYPE_UNIQUELOCAL	"Unique-Local"
#define IPV6_ADDR_TYPE_SITELOCAL	"Site-Local"
#define IPV6_ADDR_UNKNOWN		"Unknown"
#endif

#define DNS_PORT 0x35
#define DHCPD_PORT 0x43
#define DHCPC_PORT 0x44

#define net_e(...)                             \
	wmlog_e("net", ##__VA_ARGS__)

#define net_d(...)                             \
	wmlog("net", ##__VA_ARGS__)

struct interface {
	struct netif netif;
	struct ip_addr ipaddr;
	struct ip_addr nmask;
	struct ip_addr gw;
};

static struct interface g_mlan;
static struct interface g_uap;
#ifdef CONFIG_P2P
static struct interface g_wfd;
#endif

err_t lwip_netif_init(struct netif *netif);
err_t lwip_netif_uap_init(struct netif *netif);
#ifdef CONFIG_P2P
err_t lwip_netif_wfd_init(struct netif *netif);
#endif
void handle_data_packet(const t_u8 interface, const t_u8 *rcvdata,
			const t_u16 datalen);

extern void stats_udp_bcast_display();

#ifdef CONFIG_IPV6
char *ipv6_addr_state_to_desc(unsigned char addr_state)
{
	if (ip6_addr_istentative(addr_state))
		return IPV6_ADDR_STATE_TENTATIVE;
	else if (ip6_addr_ispreferred(addr_state))
		return IPV6_ADDR_STATE_PREFERRED;
	else if (ip6_addr_isinvalid(addr_state))
		return IPV6_ADDR_STATE_INVALID;
	else if (ip6_addr_isvalid(addr_state))
		return IPV6_ADDR_STATE_VALID;
	else if (ip6_addr_isdeprecated(addr_state))
		return IPV6_ADDR_STATE_DEPRECATED;
	else
		return IPV6_ADDR_UNKNOWN;

}

char *ipv6_addr_type_to_desc(struct ipv6_config *ipv6_conf)
{
	if (ip6_addr_islinklocal((ip6_addr_t *)ipv6_conf->address))
		return IPV6_ADDR_TYPE_LINKLOCAL;
	else if (ip6_addr_isglobal((ip6_addr_t *)ipv6_conf->address))
		return IPV6_ADDR_TYPE_GLOBAL;
	else if (ip6_addr_isuniquelocal((ip6_addr_t *)ipv6_conf->address))
		return IPV6_ADDR_TYPE_UNIQUELOCAL;
	else if (ip6_addr_issitelocal((ip6_addr_t *)ipv6_conf->address))
		return IPV6_ADDR_TYPE_SITELOCAL;
	else
		return IPV6_ADDR_UNKNOWN;
}
#endif /* CONFIG_IPV6 */

int net_dhcp_hostname_set(char *hostname)
{
	netif_set_hostname(&g_mlan.netif, hostname);
	return WM_SUCCESS;
}

void net_ipv4stack_init()
{
	static bool tcpip_init_done;
	if (tcpip_init_done)
		return;
	net_d("Initializing TCP/IP stack");
	tcpip_init(NULL, NULL);
	tcpip_init_done = true;
}

#ifdef CONFIG_IPV6
void net_ipv6stack_init(struct netif *netif)
{
	uint8_t mac[6];

	netif->flags |= NETIF_IPV6_FLAG_UP;

	/* Set Multicast filter for IPV6 link local address
	 * It contains first three bytes: 0x33 0x33 0xff (fixed)
	 * and last three bytes as last three bytes of device mac */
	mac[0] = 0x33;
	mac[1] = 0x33;
	mac[2] = 0xff;
	mac[3] = netif->hwaddr[3];
	mac[4] = netif->hwaddr[4];
	mac[5] = netif->hwaddr[5];
	wifi_add_mcast_filter(mac);

	netif_create_ip6_linklocal_address(netif, 1);
	netif->ip6_autoconfig_enabled = 1;

	/* IPv6 routers use multicast IPv6 ff02::1 and MAC address
	   33:33:00:00:00:01 for router advertisements */
	mac[0] = 0x33;
	mac[1] = 0x33;
	mac[2] = 0x00;
	mac[3] = 0x00;
	mac[4] = 0x00;
	mac[5] = 0x01;
	wifi_add_mcast_filter(mac);
}

static void wm_netif_ipv6_status_callback(struct netif *n)
{
	/*	TODO: Implement appropriate functionality here*/
	net_d("Received callback on IPv6 address state change");
	wlan_wlcmgr_send_msg(WIFI_EVENT_NET_IPV6_CONFIG,
				     WIFI_EVENT_REASON_SUCCESS, NULL);
}
#endif /* CONFIG_IPV6 */

void net_wlan_init(void)
{
	static int wlan_init_done;
	int ret;
	if (!wlan_init_done) {
		net_ipv4stack_init();
		wifi_register_data_input_callback(&handle_data_packet);
		g_mlan.ipaddr.addr = INADDR_ANY;
		ret = netifapi_netif_add(&g_mlan.netif, &g_mlan.ipaddr,
					 &g_mlan.ipaddr, &g_mlan.ipaddr, NULL,
					 lwip_netif_init, tcpip_input);
		if (ret) {
			/*FIXME: Handle the error case cleanly */
			net_e("MLAN interface add failed");
		}
#ifdef CONFIG_IPV6
		net_ipv6stack_init(&g_mlan.netif);
#endif /* CONFIG_IPV6 */

		ret = netifapi_netif_add(&g_uap.netif, &g_uap.ipaddr,
					 &g_uap.ipaddr, &g_uap.ipaddr, NULL,
					 lwip_netif_uap_init, tcpip_input);
		if (ret) {
			/*FIXME: Handle the error case cleanly */
			net_e("UAP interface add failed");
		}
#ifdef CONFIG_P2P
		g_wfd.ipaddr.addr = INADDR_ANY;
		ret = netifapi_netif_add(&g_wfd.netif, &g_wfd.ipaddr,
					 &g_wfd.ipaddr, &g_wfd.ipaddr, NULL,
					 lwip_netif_wfd_init, tcpip_input);
		if (ret) {
			/*FIXME: Handle the error case cleanly */
			net_e("P2P interface add failed\r\n");
		}
#endif
		ret = netif_add_udp_broadcast_filter(DNS_PORT);
		if (ret != WM_SUCCESS) {
			net_e("Failed to add UDP broadcast filter");
		}
		ret = netif_add_udp_broadcast_filter(DHCPD_PORT);
		if (ret != WM_SUCCESS) {
			net_e("Failed to add UDP broadcast filter");
		}
		ret = netif_add_udp_broadcast_filter(DHCPC_PORT);
		if (ret != WM_SUCCESS) {
			net_e("Failed to add UDP broadcast filter");
		}
		wlan_init_done = 1;
	}

	wlan_wlcmgr_send_msg(WIFI_EVENT_NET_INTERFACE_CONFIG,
				WIFI_EVENT_REASON_SUCCESS, NULL);
	return;
}

static void wm_netif_status_callback(struct netif *n)
{
	if (n->flags & NETIF_FLAG_UP){
		if(n->flags & NETIF_FLAG_DHCP){ 
			if (n->ip_addr.addr != INADDR_ANY) {
				wlan_wlcmgr_send_msg(WIFI_EVENT_NET_DHCP_CONFIG, 
						     WIFI_EVENT_REASON_SUCCESS, NULL);
			} else {
				wlan_wlcmgr_send_msg(WIFI_EVENT_NET_DHCP_CONFIG, 
						     WIFI_EVENT_REASON_FAILURE, NULL);
			}
		} else {
#ifdef CONFIG_AUTOIP
			if ((n->autoip) &&
				(n->autoip->state == AUTOIP_STATE_BOUND))
				wlan_wlcmgr_send_msg(WIFI_EVENT_NET_DHCP_CONFIG,
					WIFI_EVENT_REASON_SUCCESS, NULL);
#else
			wlan_wlcmgr_send_msg(WIFI_EVENT_NET_DHCP_CONFIG,
					     WIFI_EVENT_REASON_FAILURE, NULL);
#endif
		}
#ifdef CONFIG_AUTOIP
	} else if (!(n->flags & NETIF_FLAG_DHCP) &&
		 (n->autoip->state == AUTOIP_STATE_OFF)) {
#else
	} else if (!(n->flags & NETIF_FLAG_DHCP)) {
#endif
		wlan_wlcmgr_send_msg(WIFI_EVENT_NET_DHCP_CONFIG,
				     WIFI_EVENT_REASON_FAILURE, NULL);
	}
}

static int check_iface_mask(void *handle, uint32_t ipaddr)
{
	uint32_t interface_ip, interface_mask;
	net_get_if_ip_addr(&interface_ip, handle);
	net_get_if_ip_mask(&interface_mask, handle);
	if (interface_ip > 0)
		if ((interface_ip & interface_mask) ==
		    (ipaddr & interface_mask))
			return WM_SUCCESS;
	return -WM_FAIL;
}

void *net_ip_to_interface(uint32_t ipaddr)
{
	int ret;
	void *handle;
	/* Check mlan handle */
	handle = net_get_mlan_handle();
	ret = check_iface_mask(handle, ipaddr);
	if (ret == WM_SUCCESS)
		return handle;

	/* Check uap handle */
	handle = net_get_uap_handle();
	ret = check_iface_mask(handle, ipaddr);
	if (ret == WM_SUCCESS)
		return handle;

	/* If more interfaces are added then above check needs to done for
	 * those newly added interfaces
	 */
	return NULL;
}

void *net_sock_to_interface(int sock)
{
	struct sockaddr_in peer;
	unsigned long peerlen = sizeof(peer);
	void *req_iface = NULL;

	getpeername(sock, (struct sockaddr *)&peer, &peerlen);
	req_iface = net_ip_to_interface(peer.sin_addr.s_addr);
	return req_iface;
}

void *net_get_sta_handle(void)
{
	return &g_mlan;
}

void *net_get_uap_handle(void)
{
	return &g_uap;
}

#ifdef CONFIG_P2P
void *net_get_wfd_handle(void)
{
	return &g_wfd;
}
#endif

void net_interface_up(void *intrfc_handle)
{
	struct interface *if_handle = (struct interface *)intrfc_handle;
	netifapi_netif_set_up(&if_handle->netif);
}

void net_interface_down(void *intrfc_handle)
{
	struct interface *if_handle = (struct interface *)intrfc_handle;
	netifapi_netif_set_down(&if_handle->netif);
}

#ifdef CONFIG_IPV6
void net_interface_deregister_ipv6_callback(void *intrfc_handle)
{
	struct interface *if_handle = (struct interface *)intrfc_handle;
	if (intrfc_handle == &g_mlan)
		netif_set_ipv6_status_callback(&if_handle->netif, NULL);
}
#endif

void net_interface_dhcp_stop(void *intrfc_handle)
{
	struct interface *if_handle = (struct interface *)intrfc_handle;
	netifapi_dhcp_stop(&if_handle->netif);
	netif_set_status_callback(&if_handle->netif,
				NULL);
}

int net_configure_address(struct wlan_ip_config *addr, void *intrfc_handle)
{
	if (!addr)
		return -WM_E_INVAL;
	if (!intrfc_handle)
		return -WM_E_INVAL;

	struct interface *if_handle = (struct interface *)intrfc_handle;

	net_d("configuring interface %s (with %s)",
		(if_handle == &g_mlan) ? "mlan" :
#ifdef CONFIG_P2P
		(if_handle == &g_uap) ? "uap" : "wfd",
#else
		"uap",
#endif
		(addr->ipv4.addr_type == ADDR_TYPE_DHCP)
		? "DHCP client" : "Static IP");
	netifapi_netif_set_down(&if_handle->netif);

	/* De-register previously registered DHCP Callback for correct
	 * address configuration.
	 */
	netif_set_status_callback(&if_handle->netif,
				NULL);
#ifdef CONFIG_IPV6
	if (if_handle == &g_mlan) {
		netif_set_ipv6_status_callback(&if_handle->netif,
			wm_netif_ipv6_status_callback);
		/* Explicitly call this function so that the linklocal address
		 * gets updated even if the interface does not get any IPv6
		 * address in its lifetime */
		wm_netif_ipv6_status_callback(&if_handle->netif);
	}
#endif
	if (if_handle == &g_mlan)
		netifapi_netif_set_default(&if_handle->netif);
	switch (addr->ipv4.addr_type) {
	case ADDR_TYPE_STATIC:
		if_handle->ipaddr.addr = addr->ipv4.address;
		if_handle->nmask.addr = addr->ipv4.netmask;
		if_handle->gw.addr = addr->ipv4.gw;
		netifapi_netif_set_addr(&if_handle->netif, &if_handle->ipaddr,
					&if_handle->nmask, &if_handle->gw);
		netifapi_netif_set_up(&if_handle->netif);
		break;

	case ADDR_TYPE_DHCP:
		/* Reset the address since we might be transitioning from static to DHCP */
		memset(&if_handle->ipaddr, 0, sizeof(struct ip_addr));
		memset(&if_handle->nmask, 0, sizeof(struct ip_addr));
		memset(&if_handle->gw, 0, sizeof(struct ip_addr));
		netifapi_netif_set_addr(&if_handle->netif, &if_handle->ipaddr,
				&if_handle->nmask, &if_handle->gw);

		netif_set_status_callback(&if_handle->netif,
					wm_netif_status_callback);
		netifapi_dhcp_start(&if_handle->netif);
		break;
	case ADDR_TYPE_LLA:
		/* For dhcp, instead of netifapi_netif_set_up, a
		   netifapi_dhcp_start() call will be used */
		net_e("Not supported as of now...");
		break;
	default:
		break;
	}
	/* Finally this should send the following event. */
	if ((if_handle == &g_mlan)
#ifdef CONFIG_P2P
	    ||
	    ((if_handle == &g_wfd) && (netif_get_bss_type() == BSS_TYPE_STA))
#endif
	    ) {
		wlan_wlcmgr_send_msg(WIFI_EVENT_NET_STA_ADDR_CONFIG,
					WIFI_EVENT_REASON_SUCCESS, NULL);

		/* XXX For DHCP, the above event will only indicate that the
		 * DHCP address obtaining process has started. Once the DHCP
		 * address has been obtained, another event,
		 * WD_EVENT_NET_DHCP_CONFIG, should be sent to the wlcmgr.
		 */
	} else if ((if_handle == &g_uap)
#ifdef CONFIG_P2P
	    ||
	    ((if_handle == &g_wfd) && (netif_get_bss_type() == BSS_TYPE_UAP))
#endif
	    ) {
		wlan_wlcmgr_send_msg(WIFI_EVENT_UAP_NET_ADDR_CONFIG,
					WIFI_EVENT_REASON_SUCCESS, NULL);
	}

	return WM_SUCCESS;
}

int net_get_if_addr(struct wlan_ip_config *addr, void *intrfc_handle)
{
	struct ip_addr tmp;
	struct interface *if_handle = (struct interface *)intrfc_handle;

	addr->ipv4.address = if_handle->netif.ip_addr.addr;
	addr->ipv4.netmask = if_handle->netif.netmask.addr;
	addr->ipv4.gw = if_handle->netif.gw.addr;

	tmp = dns_getserver(0);
	addr->ipv4.dns1 = tmp.addr;
	tmp = dns_getserver(1);
	addr->ipv4.dns2 = tmp.addr;

	return WM_SUCCESS;
}

#ifdef CONFIG_IPV6
int net_get_if_ipv6_addr(struct wlan_ip_config *addr, void *intrfc_handle)
{
	struct interface *if_handle = (struct interface *)intrfc_handle;
	int i;

	for (i = 0; i < MAX_IPV6_ADDRESSES; i++) {
		memcpy(addr->ipv6[i].address,
			if_handle->netif.ip6_addr[i].addr, 16);
		addr->ipv6[i].addr_state = if_handle->netif.ip6_addr_state[i];
	}
	/* TODO carry out more processing based on IPv6 fields in netif */
	return 0;
}

int net_get_if_ipv6_pref_addr(struct wlan_ip_config *addr, void *intrfc_handle)
{
	int i, ret = 0;
	struct interface *if_handle = (struct interface *)intrfc_handle;

	for (i = 0; i < MAX_IPV6_ADDRESSES; i++) {
		if (if_handle->netif.ip6_addr_state[i] == IP6_ADDR_PREFERRED) {
			memcpy(addr->ipv6[ret++].address,
				if_handle->netif.ip6_addr[i].addr, 16);
		}
	}
	return ret;
}
#endif /* CONFIG_IPV6 */

int net_get_if_ip_addr(uint32_t *ip, void *intrfc_handle)
{
	struct interface *if_handle = (struct interface *)intrfc_handle;

	*ip = if_handle->netif.ip_addr.addr;
	return WM_SUCCESS;
}

int net_get_if_ip_mask(uint32_t *nm, void *intrfc_handle)
{
	struct interface *if_handle = (struct interface *)intrfc_handle;

	*nm = if_handle->netif.netmask.addr;
	return WM_SUCCESS;
}

void net_configure_dns(struct wlan_ip_config *ip, enum wlan_bss_role role)
{
	struct ip_addr tmp;

	if (ip->ipv4.addr_type == ADDR_TYPE_STATIC) {
		if (role != WLAN_BSS_ROLE_UAP) {
			if (ip->ipv4.dns1 == 0)
				ip->ipv4.dns1 = ip->ipv4.gw;
			if (ip->ipv4.dns2 == 0)
				ip->ipv4.dns2 = ip->ipv4.dns1;
		}
		tmp.addr = ip->ipv4.dns1;
		dns_setserver(0, &tmp);
		tmp.addr = ip->ipv4.dns2;
		dns_setserver(1, &tmp);
	}

	/* DNS MAX Retries should be configured in lwip/dns.c to 3/4 */
	/* DNS Cache size of about 4 is sufficient */
}

void net_sockinfo_dump()
{
	stats_sock_display();
}

void net_stat(char *name)
{
	stats_display(name);
	stats_udp_bcast_display();
}

u32_t sys_now()
{
	return os_ticks_get();
}
