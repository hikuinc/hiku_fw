/*
 *  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _APP_NETWORK_CONFIG_H
#define _APP_NETWORK_CONFIG_H

#include "app_psm.h"

#define APP_NETWORK_PSM_NOT_PROVISIONED     "0"
#define APP_NETWORK_PSM_PROVISIONED         "1"

#ifndef APP_NO_PSM_CONFIG

/* psm details   */
/* Max WPA2 passphrase can be upto 64 ASCII chars */
#define MAX_PSM_VAL		65
#define PART_NAME		"refpart"
#define DEF_NET_NAME		"default"
#define VAR_NET_SSID		"network.ssid"
#define VAR_NET_BSSID		"network.bssid"
#define VAR_NET_CHANNEL		"network.channel"
#define VAR_NET_SECURITY	"network.security"
#define VAR_NET_PASSPHRASE	"network.passphrase"
#define VAR_NET_POLLINT		"pollinterval"
#define VAR_NET_SENSORNAME 	"sensorname"
#define VAR_NET_CONFIGURED	"network.configured"
#define VAR_NET_LAN		"network.lan"
#define VAR_NET_IP_ADDRESS	"network.ip_address"
#define VAR_NET_SUBNET_MASK	"network.subnet_mask"
#define VAR_NET_GATEWAY		"network.gateway"
#define VAR_NET_DNS1		"network.dns1"
#define VAR_NET_DNS2		"network.dns2"
#define VAR_SCAN_DURATION	"scan_config.scan_duration"
#define VAR_SPLIT_SCAN_DELAY	"scan_config.split_scan_delay"
#define VAR_SCAN_INTERVAL	"scan_config.scan_interval"
#define VAR_SCAN_CHANNEL		"scan_config.channel"

#endif /* APP_NO_PSM_CONFIG */

/** Check if a valid network has been already loaded */
int app_check_valid_network_loaded();

/** Modifier to set network configuration */
int app_network_set_nw(struct wlan_network *network);

/** Accessor to get network configuration */
int app_network_get_nw(struct wlan_network *network);

/** Check network configuration state */
int app_network_get_nw_state();

/** Set network configuration state */
int app_network_set_nw_state(int state);

#endif
