/*
 * connection_manager.h
 *
 *  Created on: Apr 27, 2016
 *      Author: rajan-work
 */

#include "hiku_common.h"
#include <led_indicator.h>


#define HIKU_DEFAULT_WIFI_SSID "hiku"
#define HIKU_DEFAULT_WIFI_PASS "connectme"
#define FTFS_API_VERSION 100
#define FTFS_PART_NAME	"ftfs"

int connection_manager_init(void);
int hiku_get_ip_address(char *buf);
