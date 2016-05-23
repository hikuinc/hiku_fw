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

os_thread_t dhcpd_thread;
static os_thread_stack_define(dhcp_stack, 1024);
static bool dhcpd_running;
/*
 * API
 */

int dhcp_server_start(void *intrfc_handle)
{
	int ret;

	dhcp_d("DHCP server start request");
	if (dhcpd_running || dhcp_server_init(intrfc_handle))
		return -WM_E_DHCPD_SERVER_RUNNING;

	ret = os_thread_create(&dhcpd_thread, "dhcp-server", dhcp_server, 0,
			       &dhcp_stack, OS_PRIO_3);
	if (ret) {
		dhcp_free_allocations();
		return -WM_E_DHCPD_THREAD_CREATE;
	}

	dhcpd_running = 1;
	return WM_SUCCESS;
}

void dhcp_server_stop(void)
{
	dhcp_d("DHCP server stop request");
	if (dhcpd_running) {
		if (dhcp_send_halt() != WM_SUCCESS) {
			dhcp_w("failed to send halt to DHCP thread");
			return;
		}
		if (os_thread_delete(&dhcpd_thread) != WM_SUCCESS)
			dhcp_w("failed to delete thread");
		dhcpd_running = 0;
	} else {
		dhcp_w("server not dhcpd_running.");
	}
}

