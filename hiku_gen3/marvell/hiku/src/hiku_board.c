/*
 * hiku_board.c
 *
 *  Created on: May 19, 2016
 *      Author: rajan-work
 */

#include "hiku_board.h"
#include <wifi-decl.h>

#define SERIAL_PREFIX_LEN 4
#define SERIAL_PREFIX "7000"
#define MAC_ADDRESS_STR_LEN 12

int hiku_board_init(void)
{
	char buf[16];
	hiku_log("Initializing!!!\r\n");
	hiku_board_get_serial(buf);
	return WM_SUCCESS;
}

int hiku_board_get_serial(char * buf)
{
	static char serial[SERIAL_PREFIX_LEN + MAC_ADDRESS_STR_LEN] = "0000000000000000";

	if (serial[0] == '0')
	{
		unsigned char mac[MLAN_MAC_ADDR_LENGTH];
		if (wlan_get_mac_address(mac) != WM_SUCCESS)
		{
			hiku_log("Failed to read mac address!!\r\n");
			return -WM_FAIL;
		}

		sprintf(serial,"%s%02x%02x%02x%02x%02x%02x",SERIAL_PREFIX, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	}

	strncpy(buf,serial, strlen(serial));
	hiku_log("Returning Serial: %s",serial);

	return WM_SUCCESS;
}
