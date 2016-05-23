
/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/** dhcp-server-main.c: CLI based APIs for the DHCP Server
 */
#include <string.h>

#include <wmstdio.h>
#include <wm_os.h>
#include <wm_net.h>
#include <cli.h>
#include <cli_utils.h>
#include <dhcp-server.h>

#include "dhcp-priv.h"

/*
 * Command-Line Interface
 */

#define WAIT_FOR_UAP_START	5
extern uint32_t dhcp_address_timeout;
extern struct dhcp_server_data dhcps;
void dhcp_stat(int argc, char **argv)
{
	int i = 0;
	wmprintf("DHCP Server Lease Duration : %d seconds\r\n",
		 (int)dhcp_address_timeout);
	if (dhcps.count_clients == 0) {
		wmprintf("No IP-MAC mapping stored\r\n");
	} else {
		wmprintf("Client IP\tClient MAC\r\n");
		for (i = 0; i < dhcps.count_clients && i < MAC_IP_CACHE_SIZE;
		     i++) {
			wmprintf("%s\t%02X:%02X:%02X:%02X:%02X:%02X\r\n",
				 inet_ntoa(dhcps.ip_mac_mapping[i].client_ip),
				 dhcps.ip_mac_mapping[i].client_mac[0],
				 dhcps.ip_mac_mapping[i].client_mac[1],
				 dhcps.ip_mac_mapping[i].client_mac[2],
				 dhcps.ip_mac_mapping[i].client_mac[3],
				 dhcps.ip_mac_mapping[i].client_mac[4],
				 dhcps.ip_mac_mapping[i].client_mac[5]);
		}
	}
}

static struct cli_command dhcp_cmds[] = {
	{"dhcp-stat", NULL, dhcp_stat},
};

int dhcpd_cli_init(void)
{
	int i;

	for (i = 0; i < sizeof(dhcp_cmds) / sizeof(struct cli_command); i++)
		if (cli_register_command(&dhcp_cmds[i]))
			return -WM_E_DHCPD_REGISTER_CMDS;

	return WM_SUCCESS;
}
