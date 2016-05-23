/*
 *  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* app_mdns_services.c: mDNS-SD can be used for device discovery by the
 * application framework. This file defines the functions to initialize, start
 * and stop the mDNS-SD service.
 */

#include <wmstdio.h>
#include <wm_net.h>
#include <mdns.h>
#include <app_framework.h>

#include "app_ctrl.h"
#include "app_dbg.h"

static const char *md_domain = "local";
static bool mdns_started;

/* Refresh ip address and hostname */
void app_mdns_refresh_hostname(char *hostname)
{
	mdns_set_hostname(hostname);
}

/* The network is up, start the mDNS-SD service */
void app_mdns_start(char *hostname)
{
	if (mdns_started)
		return;
	/* Start the mdns thread */
	mdns_start(md_domain, hostname);
	mdns_started = 1;
}

/* The network is down, stop the mdns service */
void app_mdns_stop(void)
{
	mdns_started = 0;
	/* Stop the mdns thread */
	mdns_stop();

	/* Nothing to unregister */
}

int app_mdns_add_service_arr(struct mdns_service *services[], void *iface)
{
	int ret;
	ret = mdns_announce_service_arr(services, iface);
	os_thread_sleep(os_msec_to_ticks(1000));
	return ret;
}

/* Remove interface and services on them from mdns */
int app_mdns_remove_service_arr(struct mdns_service *services[], void *iface)
{
	int ret;
	ret = mdns_deannounce_service_arr(services, iface);
	os_thread_sleep(os_msec_to_ticks(1000));
	return ret;
}

int app_mdns_add_service(struct mdns_service *srv, void *iface)
{
	int ret;
	ret = mdns_announce_service(srv, iface);
	os_thread_sleep(os_msec_to_ticks(1000));
	return ret;
}

int app_mdns_remove_service(struct mdns_service *srv, void *iface)
{
	int ret;
	ret = mdns_deannounce_service(srv, iface);
	os_thread_sleep(os_msec_to_ticks(1000));
	return ret;
}

int app_mdns_remove_service_all(void *iface)
{
	int ret;
	ret = mdns_deannounce_service_all(iface);
	os_thread_sleep(os_msec_to_ticks(1000));
	return ret;
}

void app_mdns_iface_state_change(void *iface, enum iface_state state)
{
	mdns_iface_state_change(iface, state);
	os_thread_sleep(os_msec_to_ticks(1000));
}
