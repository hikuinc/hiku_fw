/*
 * connection_manager.h
 *
 *  Created on: Apr 27, 2016
 *      Author: rajan-work
 */

#include "hiku_common.h"
#include <led_indicator.h>


#include <wm_os.h>
#include <wmstdio.h>
#include <wmtime.h>
#include <wmsdk.h>
#include <board.h>
#include <aws_iot_mqtt_interface.h>
#include <aws_iot_shadow_interface.h>
#include <aws_utils.h>
/* configuration parameters */
#include <aws_iot_config.h>

enum state {
	AWS_CONNECTED = 1,
	AWS_RECONNECTED,
	AWS_DISCONNECTED
};


#define HIKU_DEFAULT_WIFI_SSID "hiku"
#define HIKU_DEFAULT_WIFI_PASS "connectme"
#define FTFS_API_VERSION 100
#define FTFS_PART_NAME	"ftfs"

int connection_manager_init(void);
int hiku_get_ip_address(char *buf);
