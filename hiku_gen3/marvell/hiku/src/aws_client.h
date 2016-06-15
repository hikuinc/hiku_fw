/*
 * aws_client.h
 *
 *  Created on: May 24, 2016
 *      Author: rajan-work
 */

#ifndef SRC_AWS_CLIENT_H_
#define SRC_AWS_CLIENT_H_

#include "hiku_common.h"


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

int aws_client_init(void);

void aws_wlan_event_normal_link_lost(void *data);
void aws_wlan_event_normal_connect_failed(void *data);
void aws_wlan_event_normal_connected(void *data);

#endif /* SRC_AWS_CLIENT_H_ */
