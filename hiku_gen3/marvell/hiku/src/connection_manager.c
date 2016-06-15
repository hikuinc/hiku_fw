/*
 * connection_manager.c
 *
 *  Created on: Apr 27, 2016
 *      Author: rajan-work
 */

#include "connection_manager.h"
#include <wlan.h>
#include "http_manager.h"
#include "hiku_board.h"
#ifdef FEATURE_HIKU_AWS
#include "aws_client.h"
#endif /*FEATURE_HIKU_AWS */


/** Connection and WiFi event handler function forward declarations */
int common_event_handler(int event, void *data);
static void event_wlan_init_done(void *data);
static void event_prov_done(void *data);
static void event_normal_dhcp_renew(void *data);
static void event_wifi_connected(void *data);
static void event_wifi_connecting(void *data);
/** End of all the forward declarations related to WiFi/Connectivity Event Handlers */


/** Global Variable definitions */

static struct wlan_ip_config addr;

// the GPIO connected to the LED
static output_gpio_cfg_t connection_led = {
    .gpio = STATUS_LED,
    .type = GPIO_ACTIVE_LOW,
};

/** Function implementations */

/**
 * function: common_event_handler
 * @param event being handled
 * @param data for the specific event
 * @return if the handling is SUCCESS or FAIL
 */
int common_event_handler(int event, void *data)
{
	hiku_c("Received Event: %s\r\n",app_ctrl_event_name(event));
	switch(event)
	{
		case AF_EVT_WLAN_INIT_DONE:
			event_wlan_init_done(data);
			break;
		case AF_EVT_PROV_CLIENT_DONE:
			event_prov_done(data);
			break;
		case AF_EVT_NORMAL_DHCP_RENEW:
			event_normal_dhcp_renew(data);
			break;
		case AF_EVT_NORMAL_CONNECTED:
			event_wifi_connected(data);
#ifdef FEATURE_HIKU_AWS
			aws_wlan_event_normal_connected(data);
#endif /* FEATURE_HIKU_AWS */
			break;
		case AF_EVT_NORMAL_CONNECTING:
			event_wifi_connecting(data);
			break;
		case AF_EVT_NORMAL_CONNECT_FAILED:
#ifdef FEATURE_HIKU_AWS
			aws_wlan_event_normal_connect_failed(data);
#endif /* FEATURE_HIKU_AWS */
			break;
		case AF_EVT_NORMAL_LINK_LOST:
#ifdef FEATURE_HIKU_AWS
			aws_wlan_event_normal_link_lost(data);
#endif /* FEATURE_HIKU_AWS */
			break;
		default:
			break;
	}
	return WM_SUCCESS;
}

/**
 * function: event_wlan_init_done
 * description: This function will take care of actions pertaining to connectivity
 * 				Once the WiFi stack is initialized.  If a provision profile is not there
 * 				this will start the EzConnect to do provisioning by making this as a Soft AP
 * @param data for the event
 * @return none
 */
static void event_wlan_init_done(void *data)
{
	int provisioned = (int)data;
	//app_ftfs_init(FTFS_API_VERSION, FTFS_PART_NAME, &fs_handle);
	char buf[16];
	hiku_board_get_serial(buf);

	if (http_manager_init() != WM_SUCCESS)
	{
		hiku_c("Failed to initialize HTTP Manager!!\r\n");
	}

	hiku_c("Event: WLAN_INIT_DONE provisioned=%d\r\n", provisioned);

	if (provisioned)
	{
		hiku_c("Starting station\r\n");
		app_sta_start();
	}
	else
	{
		led_on(connection_led);
		hiku_c("No provisioned\r\n");
		app_ezconnect_provisioning_start(HIKU_DEFAULT_WIFI_SSID, (unsigned char*)HIKU_DEFAULT_WIFI_PASS,strlen(HIKU_DEFAULT_WIFI_PASS));
	}
}

/**
 * function: event_prov_done
 * description: This function will stop the EzConnect provisioning service as soon as it succeeds
 * 				Provisioning via the web service
 * @param data for the event
 * @return none
 */
static void event_prov_done(void *data)
{
	hiku_c("Provisioning Completed!\r\n");
	app_ezconnect_provisioning_stop();
}

/**
 * function: event_normal_dhcp_renew
 * description: This function will take care of actions once a DHCP renewal of IP happens
 * @param data for the event
 * @return none
 */
static void event_normal_dhcp_renew(void *data)
{
	hiku_c("DHCP IP renewal!\r\n");
}

static void event_wifi_connecting(void *data)
{
	led_fast_blink(connection_led);
}

static void event_wifi_connected(void *data)
{
	led_off(connection_led);
	led_slow_blink(connection_led);

	if (wlan_get_address(&addr) != WM_SUCCESS)
	{
		hiku_c("Failed to retrieve the IP Address!\r\n");
	}
	else
	{

		uint32_t ipv4 = addr.ipv4.address;
		char buf[65];
		snprintf(buf, sizeof(buf), "%s",
			 inet_ntoa(ipv4));
		hiku_c("IP Address:%s\r\n", buf);
	}
}

int hiku_get_ip_address(char *buf)
{
	uint32_t ipv4 = addr.ipv4.address;
	snprintf(buf, sizeof(buf), "%s",
		 inet_ntoa(ipv4));
	return WM_SUCCESS;
}

/**
 * function: connection_manager_init
 * description: This function is the entry point into the connection manager module.  This will initalize the WiFi stack
 * @param none
 * @return none
 */
int connection_manager_init(void)
{
	cli_init();
	psm_cli_init();

	if (app_framework_start(common_event_handler) != WM_SUCCESS)
	{
		hiku_c("Failed to start the application framework\r\n");
		return -WM_FAIL;
	}
	return WM_SUCCESS;
}
