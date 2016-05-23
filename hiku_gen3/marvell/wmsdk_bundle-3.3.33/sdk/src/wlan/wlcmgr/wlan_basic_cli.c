/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wlan.h>
#include <cli.h>

void test_wlan_version(int argc, char **argv)
{
	char *version_str;

	version_str = wlan_get_firmware_version_ext();

	wmprintf("WLAN Driver Version   : %s\r\n", WLAN_DRV_VERSION);
	wmprintf("WLAN Firmware Version : %s\r\n", version_str);
}

static void test_wlan_get_mac_address(int argc, char **argv)
{
	uint8_t mac[6];

	wmprintf("MAC address\r\n");
	if (wlan_get_mac_address(mac))
		wmprintf("Error: unable to retrieve MAC address\r\n");
	else
		wmprintf("%02X:%02X:%02X:%02X:%02X:%02X\r\n",
			       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#ifdef CONFIG_P2P
	wmprintf("P2P MAC address\r\n");
	if (wlan_get_wfd_mac_address(mac))
		wmprintf("Error: unable to retrieve P2P MAC address\r\n");
	else
		wmprintf("%02X:%02X:%02X:%02X:%02X:%02X\r\n",
			       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#endif
}

static struct cli_command wlan_basic_commands[] = {
	{"wlan-version", NULL, test_wlan_version},
	{"wlan-mac", NULL, test_wlan_get_mac_address},
};

int wlan_basic_cli_init(void)
{
	int i;

	for (i = 0;	i < sizeof(wlan_basic_commands) \
		/ sizeof(struct cli_command); i++)
		if (cli_register_command(&wlan_basic_commands[i]))
			return WLAN_ERROR_ACTION;

	return WLAN_ERROR_NONE;
}
