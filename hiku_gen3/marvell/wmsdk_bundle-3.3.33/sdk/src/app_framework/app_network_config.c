/*
 *  Copyright 2008-2015 Marvell International Ltd.
 *  All Rights Reserved.
 */

/* app_network_config.c: This file performs the job of maintaining network
 * configuration information in the psm. The network configuration information
 * includes the current state of the device (provisioning v/s normal) and if in
 * normal mode, the details of the configured AP.
 */

#include <string.h>

#include <cli_utils.h>
#include <wm_utils.h>
#include <wlan.h>
#include <psm.h>
#include <psm-v2.h>
#include <wm_net.h>
#include <app_framework.h>

#include "app_network_config.h"
#include "app_dbg.h"
#include "app_ctrl.h"

#ifndef APP_NO_PSM_CONFIG

#define LAN_TYPE_STATIC    "STATIC"
#define LAN_TYPE_AUTOIP    "AUTOIP"
#define LAN_TYPE_DHCP      "DYNAMIC"

/* Get the current network state that is noted in the PSM. The state could be
 * either of APP_NETWORK_PROVISIONED and APP_NETWORK_NOT_PROVISIONED
 */
WEAK int app_network_get_nw_state(void)
{
	char psm_val[MAX_PSM_VAL];
	int ret = psm_get_variable_str(app_psm_hnd, VAR_NET_CONFIGURED,
				psm_val, sizeof(psm_val));
	if (ret <= 0)
		return APP_NETWORK_NOT_PROVISIONED;

	app_d("app_network_config: value of " VAR_NET_CONFIGURED
		" is :%s:", psm_val);
	if (!strcmp(psm_val, APP_NETWORK_PSM_PROVISIONED))
		return APP_NETWORK_PROVISIONED;
	else
		return APP_NETWORK_NOT_PROVISIONED;
}

/* Get the currently configured wireless network from the PSM */
WEAK int app_network_get_nw(struct wlan_network *network)
{
	char psm_val[MAX_PSM_VAL];

	memset(network, 0, sizeof(*network));
	memcpy(network->name, DEF_NET_NAME, strlen(DEF_NET_NAME) + 1);

	/*
	 * Perform error handling only for those fields that are a must. For
	 * other fields, make safe assumptions if the information doesn't exist
	 * in psm.
	 */
	int ret = psm_get_variable_str(app_psm_hnd, VAR_NET_SSID,
				   network->ssid, sizeof(network->ssid));

#ifndef APP_USE_SPECIFIC_AP_INFO
#define DEF_BSSID    "00:00:00:00:00:00"
	get_mac(DEF_BSSID, network->bssid, ':');
	network->channel = 0;
#else
	ret = psm_get_variable_str(app_psm_hnd, VAR_NET_BSSID,
			       psm_val, sizeof(psm_val));
	get_mac(psm_val, network->bssid, ':');

	ret = psm_get_variable_str(app_psm_hnd, VAR_NET_CHANNEL,
			 psm_val, sizeof(psm_val));

	network->channel = atoi(psm_val);
	app_d("get_psm: read channel %d", network->channel);
#endif

	network->type = WLAN_BSS_TYPE_STA;
	network->role = WLAN_BSS_ROLE_STA;

	int var_size;
	var_size = psm_get_variable_str(app_psm_hnd, VAR_NET_SECURITY,
			       psm_val, sizeof(psm_val));
	if (var_size > 0) {
		ret = 0;
	} else
		ret = var_size;

	network->security.type = (enum wlan_security_type)atoi(psm_val);
	if (network->security.type != WLAN_SECURITY_NONE) {
		if (network->security.type != WLAN_SECURITY_WEP_OPEN &&
		    network->security.type != WLAN_SECURITY_WEP_SHARED) {
			var_size = psm_get_variable_str(app_psm_hnd,
						VAR_NET_PASSPHRASE,
						network->security.psk,
						sizeof(network->security.psk));
			if (var_size <= 0)
				ret |= var_size;
			network->security.psk_len =
				strlen(network->security.psk); 
		} else {
			/* Converting the wep key written in hex back to binary
			   form */
			var_size = psm_get_variable_str(app_psm_hnd,
						VAR_NET_PASSPHRASE,
						psm_val, sizeof(psm_val));
			if (var_size <= 0)
				ret |= var_size;
			network->security.psk_len = 
				hex2bin((unsigned char*)psm_val,
					(unsigned char*)network->security.psk, 
					sizeof(psm_val));
		}
	}
	/* Connect specific to the SSID */
	network->ssid_specific = 1;

	psm_val[0] = 0;
	var_size = psm_get_variable_str(app_psm_hnd, VAR_NET_LAN,
				    psm_val, sizeof(psm_val));
	if (!strcasecmp(psm_val, LAN_TYPE_STATIC)) {

		network->ip.ipv4.addr_type = ADDR_TYPE_STATIC;

		psm_val[0] = 0;
		var_size = psm_get_variable_str(app_psm_hnd,
			VAR_NET_IP_ADDRESS,
				 psm_val, sizeof(psm_val));
		network->ip.ipv4.address = net_inet_aton(psm_val);

		psm_val[0] = 0;
		var_size = psm_get_variable_str(app_psm_hnd,
			VAR_NET_SUBNET_MASK,
			psm_val, sizeof(psm_val));
		network->ip.ipv4.netmask = net_inet_aton(psm_val);

		psm_val[0] = 0;
		var_size = psm_get_variable_str(app_psm_hnd, VAR_NET_GATEWAY,
			psm_val, sizeof(psm_val));
		network->ip.ipv4.gw = net_inet_aton(psm_val);

		psm_val[0] = 0;
		var_size = psm_get_variable_str(app_psm_hnd, VAR_NET_DNS1,
			psm_val, sizeof(psm_val));
		network->ip.ipv4.dns1 = net_inet_aton(psm_val);

		psm_val[0] = 0;
		var_size = psm_get_variable_str(app_psm_hnd, VAR_NET_DNS2,
			psm_val, sizeof(psm_val));
		network->ip.ipv4.dns2 = net_inet_aton(psm_val);
	} else if (!strcasecmp(psm_val, LAN_TYPE_AUTOIP))
		network->ip.ipv4.addr_type = ADDR_TYPE_LLA;
	else
		network->ip.ipv4.addr_type = ADDR_TYPE_DHCP;

	if (ret != 0)
		ret = -WM_E_AF_NW_RD;
	return ret;
}

/* Write the current network state in the PSM. The state could be either of
 * APP_NETWORK_PROVISIONED, APP_NETWORK_NOT_PROVISIONED
 */
WEAK int app_network_set_nw_state(int state)
{
	int ret = -WM_FAIL;

	if (state == APP_NETWORK_NOT_PROVISIONED)
		ret = psm_set_variable_str(app_psm_hnd, VAR_NET_CONFIGURED,
			      APP_NETWORK_PSM_NOT_PROVISIONED);
	else if (state == APP_NETWORK_PROVISIONED)
		ret = psm_set_variable_str(app_psm_hnd, VAR_NET_CONFIGURED,
			      APP_NETWORK_PSM_PROVISIONED);
	else {
		app_e("app_network_config: ERROR invalid state ", state);
	}

	if (ret != WM_SUCCESS)
		ret = -WM_E_AF_NW_WR;
	return ret;
}


/* Write this as the currently configured wireless network in the PSM */
WEAK int app_network_set_nw(struct wlan_network *network)
{
	int ret;
	char psm_val[MAX_PSM_VAL];
	char dest[MAX_PSM_VAL] = "\0";
	struct in_addr ip;

	if (network == NULL)
		return -WM_FAIL;

	snprintf(psm_val, sizeof(psm_val), "%s", network->ssid);
	ret = psm_set_variable_str(app_psm_hnd, VAR_NET_SSID,
			       psm_val);

	app_d("app_network_config: configuring ssid %s", network->ssid);

	snprintf(psm_val, sizeof(psm_val), "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
		 network->bssid[0], network->bssid[1], network->bssid[2],
		 network->bssid[3], network->bssid[4], network->bssid[5]);
	ret |= psm_set_variable_str(app_psm_hnd, VAR_NET_BSSID,
				psm_val);
	app_d("app_network_config: configuring bssid "
	      "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
	      network->bssid[0], network->bssid[1], network->bssid[2],
	      network->bssid[3], network->bssid[4], network->bssid[5]);

	snprintf(psm_val, sizeof(psm_val), "%d", network->channel);
	ret |= psm_set_variable_str(app_psm_hnd, VAR_NET_CHANNEL,
				psm_val);
	app_d("app_network_config: configuring channel %d",
		network->channel);

	snprintf(psm_val, sizeof(psm_val), "%d", network->security.type);
	ret |= psm_set_variable_str(app_psm_hnd, VAR_NET_SECURITY,
				psm_val);

	snprintf(psm_val, sizeof(psm_val), "%s", network->security.psk);
	if (network->security.type == WLAN_SECURITY_WEP_OPEN ||
	    network->security.type == WLAN_SECURITY_WEP_SHARED) {
		/* Converting the binary wep key to hex
		 * to write to psm */
		bin2hex((uint8_t *)network->security.psk,
			dest, network->security.psk_len, sizeof(dest));
		snprintf(psm_val, sizeof(psm_val), "%s", dest);
	} else {
		snprintf(psm_val, sizeof(psm_val), "%s",
			 network->security.psk);
	}
	ret |= psm_set_variable_str(app_psm_hnd, VAR_NET_PASSPHRASE,
				psm_val);

	if (network->ip.ipv4.addr_type == ADDR_TYPE_STATIC) {
		snprintf(psm_val, sizeof(psm_val), LAN_TYPE_STATIC);
		ret |= psm_set_variable_str(app_psm_hnd, VAR_NET_LAN,
					psm_val);

		ip.s_addr = network->ip.ipv4.address;
		snprintf(psm_val, sizeof(psm_val), "%s", inet_ntoa(ip));
		ret |= psm_set_variable_str(app_psm_hnd, VAR_NET_IP_ADDRESS,
					psm_val);

		ip.s_addr = network->ip.ipv4.netmask;
		snprintf(psm_val, sizeof(psm_val), "%s", inet_ntoa(ip));
		ret |= psm_set_variable_str(app_psm_hnd, VAR_NET_SUBNET_MASK,
					psm_val);

		ip.s_addr = network->ip.ipv4.gw;
		snprintf(psm_val, sizeof(psm_val), "%s", inet_ntoa(ip));
		ret |= psm_set_variable_str(app_psm_hnd, VAR_NET_GATEWAY,
					psm_val);

		ip.s_addr = network->ip.ipv4.dns1;
		snprintf(psm_val, sizeof(psm_val), "%s", inet_ntoa(ip));
		ret |= psm_set_variable_str(app_psm_hnd, VAR_NET_DNS1,
					psm_val);

		ip.s_addr = network->ip.ipv4.dns2;
		snprintf(psm_val, sizeof(psm_val), "%s", inet_ntoa(ip));
		ret |= psm_set_variable_str(app_psm_hnd, VAR_NET_DNS2,
					psm_val);
	} else if (network->ip.ipv4.addr_type == ADDR_TYPE_LLA) {
		snprintf(psm_val, sizeof(psm_val), LAN_TYPE_AUTOIP);
		ret |= psm_set_variable_str(app_psm_hnd, VAR_NET_LAN,
					psm_val);
	} else {
		snprintf(psm_val, sizeof(psm_val), LAN_TYPE_DHCP);
		ret |= psm_set_variable_str(app_psm_hnd, VAR_NET_LAN,
					psm_val);
	}

	ret |= psm_set_variable_str(app_psm_hnd, VAR_NET_CONFIGURED,
				APP_NETWORK_PSM_PROVISIONED);

	if (ret != 0)
		ret = -WM_E_AF_NW_WR;
	return ret;
}

#endif /* ! APP_NO_PSM_CONFIG */

/* Get the current IP Address of the module */
int app_network_ip_get(char *ipaddr)
{
	int ret;
	struct wlan_ip_config addr;

	ret = wlan_get_address(&addr);
	if (ret != WM_SUCCESS) {
		app_w("app_network_config: failed to get IP address");
		strcpy(ipaddr, "0.0.0.0");
		return -WM_FAIL;
	}

	net_inet_ntoa(addr.ipv4.address, ipaddr);
	return WM_SUCCESS;
}

