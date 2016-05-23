/*
 * ota_update.c
 *
 *  Created on: May 17, 2016
 *      Author: rajan-work
 */

#include "ota_update.h"

/** forward declarations */

static int upgrade_hiku_firmware(const char *url);
static void cmd_fw_upg(int argc, char **argv);
static int ota_fw_upgrade_cli_init(void);

/** global variable definitions */

static struct cli_command fw_upg_command = {"fw_upg", "<http_url>", cmd_fw_upg};

/** function implementations */

static int upgrade_hiku_firmware(const char *url)
{
	struct partition_entry *p = rfget_get_passive_firmware();

	hiku_o("Starting firmware update from: %s",url);

	int ret = rfget_client_mode_update(url, p, NULL);

	if (ret == WM_SUCCESS )
	{
		hiku_o("Firmware update succeeded!\r\n");
	}
	else
	{
		hiku_o("Firmware update failed!!\r\n'");
	}

	return ret;
}

static void cmd_fw_upg(int argc, char **argv)
{
	const char *url_str = argv[1];
	if (argc != 2)
	{
		hiku_o("Usage: %s <http_url>\r\n", argv[0]);
		hiku_o("Invalid number of arguments. \r\n");
		return;
	}
	upgrade_hiku_firmware(url_str);
}

static int ota_fw_upgrade_cli_init(void)
{
	return cli_register_command(&fw_upg_command);
}

int ota_update_init(void)
{
	hiku_o("Initializing OTA Upgrade Module\r\n");
	int ret = ota_fw_upgrade_cli_init();
	if (ret == WM_SUCCESS)
	{
		hiku_o("Successfully registered the cli cmd!\r\n");
	}
	else
	{
		hiku_o("Failed to register cli cmd!\r\n");
	}
	return ret;
}
