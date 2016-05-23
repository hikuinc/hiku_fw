/*
 *  Copyright (C) 2008-2016 Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <cli.h>
#include <rfget.h>

static int upgrade_my_firmware(const char *url)
{
	struct partition_entry *p = rfget_get_passive_firmware();

	int ret = rfget_client_mode_update(url, p, NULL);
	if (ret == WM_SUCCESS) {
		wmprintf("[fw_upg] Firmware update succeeded\r\n");
	} else {
		wmprintf("[fw_upg] Firmware update failed\r\n");
	}
	return ret;
}

static void cmd_fw_upg(int argc, char **argv)
{
	const char *url_str = argv[1];

	if (argc != 2) {
		wmprintf("Usage: %s <http-url>\r\n", argv[0]);
		wmprintf("Invalid number of arguments.\r\n");
		return;
	}
	upgrade_my_firmware(url_str);
}

static struct cli_command fw_upg_command = {"fw_upg", "<http_url>", cmd_fw_upg};

int fw_upgrade_cli_init()
{
	return cli_register_command(&fw_upg_command);
}
