/*
 * Copyright 2008-2015, Marvell International Ltd.
 * All Rights Reserved.
 *
 */

#ifndef __PROVISIONING_INT_H__
#define __PROVISIONING_INT_H__

#define NUM_TOKENS	30

#define J_NAME_SSID		"ssid"
#define J_NAME_BSSID		"bssid"
#define J_NAME_CHANNEL		"channel"
#define J_NAME_SECURITY		"security"
#define J_NAME_PASSPHRASE	"passphrase"
#define	J_NAME_IPV4		"ipv4"
#ifdef CONFIG_IPV6
#define	J_NAME_IPV6		"ipv6"
#define	J_NAME_SCOPE		"scope"
#endif
#define J_NAME_IPTYPE		"iptype"
#define	J_NAME_IP		"ip"
#define	J_NAME_IP_ADDR		"ipaddr"
#define J_NAME_NETMASK		"ipmask"
#define J_NAME_GW		"ipgw"
#define J_NAME_DNS1		"ipdns1"
#define J_NAME_DNS2		"ipdns2"
#define J_NAME_KEY		"key"
#define J_NAME_NETWORKS		"networks"
#define J_NAME_SCAN_DURATION	"scan_duration"
#define J_NAME_SCAN_INTERVAL	"scan_interval"
#define J_NAME_SPLIT_SCAN_DELAY	"split_scan_delay"
#define J_NAME_RSSI		"rssi"

#define user_action ", please hit the browser's back button"
#define invalid_ssid "error: invalid ssid"user_action
#define invalid_wep_key "error: invalid wep key"user_action
#define invalid_ip_settings "error: invalid IP settings"user_action
#define invalid_pin "error: invalid provisioning pin"user_action
#define invalid_key "error: invalid provisioning key"user_action
#define invalid_security "error: invalid security type"user_action
#define failed_to_set_network "error: failed to set network"user_action
#define invalid_json_element "error: invalid json element"user_action
#define no_valid_element "error: no valid element"
#define invalid_channel_list "error: invalid channel list"
#define invalid_scan_duration \
"error: scan_duration should be a value >= 50 and <= 500"
#define invalid_split_scan_delay \
"error: split_scan_delay should be a value >= 30 and <= 300"
#define invalid_scan_interval \
"error: scan_interval should be a value >= 2 and <= 60"
#define invalid_channel_number \
"error: channel number should be a value >= 0 and <= 11"
#define failed_to_set_scan_config \
"error: failed_to_set_scan_config"

#define PARSE_IP_ELEMENT(jobj, tagname, dest)		\
	do { \
		if (json_get_val_str(jobj, tagname, json_val, \
				     sizeof(json_val)) < 0) \
			goto done; \
		addr = inet_addr(json_val); \
		if (addr == -1)	\
			goto done; \
		dest = addr; \
} while (0)

enum prov_internal_event {
	EVENT_WLAN,
	EVENT_WPS,
/* Internal commands */
/* Set network from WLAN network based provisioning */
#define SET_NW_WLANNW		1
/* Set network from WPS based provisioning */
#define SET_NW_WPS		2
/* Perform WPS session attempt */
#define WPS_SESSION_ATTEMPT	3
/* Re-initialize WLAN network */
#define PROV_WLANNW_REINIT	4
/* Start Prov State Machine */
#define PROV_INIT_SM            5
/* Start Prov State Machine */
#define PROV_STOP            6
	EVENT_CMD,
};

#define MESSAGE(event, data) ((((event) & 0xff) << 8) | ((data) & 0xff))
#define MESSAGE_EVENT(message) (((message) >> 8) & 0xff)
#define MESSAGE_DATA(data) ((data) & 0xff)

extern struct site_survey survey;

struct prov_gdata {
	unsigned int state;
	struct provisioning_config *pc;
	unsigned char scan_running;
	int scan_interval;
/* Provisioning WLAN network started */
#define PROV_WLANNW_STARTED		(unsigned int)(1<<0)
/* WLAN network based provisioning successful */
#define PROV_WLANNW_SUCCESSFUL		(unsigned int)(1<<1)
	int prov_nw_state;
	struct wlan_network current_net;
	os_semaphore_t sem_current_net;

#ifdef CONFIG_WPS2
/* WPS provisioning started */
#define PROV_WPS_STARTED	(unsigned int)(1<<0)
/* WPS pushbutton attempt enabled */
#define PROV_WPS_PBC_ENABLED	(unsigned int)(1<<1)
/* WPS PIN attempt enabled */
#define PROV_WPS_PIN_ENABLED	(unsigned int)(1<<2)
/* WPS based provisioning successful */
#define PROV_WPS_SUCCESSFUL	(unsigned int)(1<<3)
	int wps_state;
	unsigned int wps_pin;
	unsigned long time_pin_expiry;
	unsigned long time_pbc_expiry;
	struct wlan_scan_result wps_session_nw;
#endif
};

#define MAX_SITES                       CONFIG_MAX_AP_ENTRIES
struct site_survey {
	struct wlan_scan_result sites[MAX_SITES];
	unsigned int num_sites;
	os_mutex_t lock;
};

/* Provisioning web handlers internal APIs */
int prov_get_state(void);

#include <wmlog.h>

#define prov_e(...)				\
	wmlog_e("prov", ##__VA_ARGS__)
#define prov_w(...)				\
	wmlog_w("prov", ##__VA_ARGS__)

#ifdef CONFIG_PROVISIONING_DEBUG
#define prov_d(...)				\
	wmlog("prov", ##__VA_ARGS__)
#else
#define prov_d(...)
#endif /* ! CONFIG_PROVISIONING_DEBUG */


#endif				/* __PROVISIONING_INT_H__ */
