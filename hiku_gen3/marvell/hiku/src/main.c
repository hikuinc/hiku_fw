/*
 * main.c
 *
 *  Created on: Apr 27, 2016
 *      Author: rajan-work
 */

#include "hiku_common.h"
#include "connection_manager.h"
#include "ota_update.h"
#include "button_manager.h"
#include "aws_client.h"

int main()
{
	/* Initialize console on uart0 */
	wmstdio_init(UART0_ID, 0);
	hiku_m("Initializing hiku!!\r\n");

	if (aws_client_init()!= WM_SUCCESS)
	{
		hiku_m("Failed to initialize AWS Client!!\r\n");
		return -WM_FAIL;
	}

	if (connection_manager_init() != WM_SUCCESS )
	{
		hiku_m("Failed to initialize Connection Manager!!\r\n");
		return -WM_FAIL;
	}

	if (ota_update_init() != WM_SUCCESS)
	{
		hiku_m("Failed to initialize OTA Update Manager!!\r\n");
		return -WM_FAIL;
	}

	if (button_manager_init() != WM_SUCCESS)
	{
		hiku_m("Failed to initialize Button Manager!!\r\n");
		return -WM_FAIL;
	}

	hiku_m("Successfully initialized hiku!!\r\n");
	return WM_SUCCESS;
}


