/*
 *  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/** provisioning.c: The provisioning state machine
 */
#include <stdio.h>
#include <string.h>

#include <wmtypes.h>
#include <wmstdio.h>
#include <wm_os.h>
#include <wm_net.h>
#include <wps.h>
#include <wlan.h>
#include <provisioning.h>
#include <dhcp-server.h>

#include "provisioning_int.h"

/* message queue and thread for state machine */
static os_queue_pool_define(provisioning_queue_data, 4 * 4);
static os_queue_t provisioning_queue;
static os_thread_stack_define(provisioning_stack, 2048);
static os_thread_t provisioning_thread;
static os_semaphore_t prov_scan_protection_sem;
os_semaphore_t os_sem_on_demand_scan;
static uint8_t is_sem_created;
enum prov_scan_policy scan_policy;
/* This instance is used to hold scan results */
struct site_survey survey;
bool is_wps_requested;
#define DEFAULT_SCAN_INTERVAL 20
/* This timeout (in milliseconds) indicates that the Wi-Fi is unresponsive
   to the wlan scan command issued by the provisioning module. The behaviour
   after this timeout might be undefined */
#define DEFAULT_SCAN_PROTECTION_TIMEOUT 30

/* Global provisioning data */
struct prov_gdata prov_g = {
	.state = PROVISIONING_STATE_INITIALIZING,
	.pc = NULL,
	.scan_running = 0,
	.scan_interval = DEFAULT_SCAN_INTERVAL,
	.prov_nw_state = 0,	/* Provisioning network not started */
#ifdef CONFIG_WPS2
	.wps_state = 0,		/* WPS not started */
	.wps_pin = 0,
	.time_pbc_expiry = 0,
	.time_pin_expiry = 0,
#endif
};

static void prov_variable_reset()
{
	prov_g.state = PROVISIONING_STATE_INITIALIZING;
	prov_g.pc = NULL;
	prov_g.scan_running = 0;
	prov_g.scan_interval = DEFAULT_SCAN_INTERVAL;
	prov_g.prov_nw_state = 0;	/* Provisioning network not started */
#ifdef CONFIG_WPS2
	prov_g.wps_state = 0;	/* WPS not started */
	prov_g.wps_pin = 0;
	prov_g.time_pbc_expiry = 0;
	prov_g.time_pin_expiry = 0;
#endif

}

int prov_get_state(void)
{
	return prov_g.state;
}

#ifdef CONFIG_WPS2
int prov_wps_pushbutton_press()
{
	if (prov_g.state == PROVISIONING_STATE_SET_CONFIG) {
		return -WM_E_PROV_SESSION_ATTEMPT;
	}

	if (prov_g.state != PROVISIONING_STATE_ACTIVE) {
		return -WM_E_PROV_INVALID_STATE;
	}

	prov_g.wps_state |= PROV_WPS_PBC_ENABLED;
	prov_g.time_pbc_expiry = os_ticks_get() + os_msec_to_ticks(120 * 1000);
	prov_d("Current time %lu expiry %lu", os_ticks_get(),
	    prov_g.time_pbc_expiry);

	is_wps_requested = true;
	return WM_SUCCESS;
}

int prov_wps_provide_pin(unsigned int pin)
{
	if (prov_g.state == PROVISIONING_STATE_SET_CONFIG) {
		return -WM_E_PROV_SESSION_ATTEMPT;
	}

	if (prov_g.state != PROVISIONING_STATE_ACTIVE) {
		return -WM_E_PROV_INVALID_STATE;
	}

	if (wps_validate_pin(pin) != WM_SUCCESS) {
		prov_e("WPS PIN checksum validation failed.\r\n");
		return -WM_E_PROV_PIN_CHECKSUM_ERR;
	}

	prov_g.wps_state |= PROV_WPS_PIN_ENABLED;
	prov_g.wps_pin = pin;
	prov_g.time_pin_expiry = os_ticks_get() + os_msec_to_ticks(120 * 1000);
	prov_d("Current time %lu expiry %lu", os_ticks_get(),
	    prov_g.time_pin_expiry);

	is_wps_requested = true;
	return WM_SUCCESS;
}

static int prov_wps_callback(enum wps_event event, void *data, uint16_t len)
{
	unsigned long msg;
	int err;

	prov_d("WPS EVENT = %d data = %p len=%d", event, data, len);

	if (event == WPS_SESSION_SUCCESSFUL) {
		prov_d("WPS_SESSION_SUCCESSFUL ");
		if (data == NULL || len != sizeof(struct wlan_network)) {
			prov_e("Invalid data for WPS SESSION SUCCESSFUL");
			return -WM_FAIL;
		}

		err = prov_set_network(PROVISIONING_WPS, data);
		if (err != WM_SUCCESS) {
			prov_e("Failed to set network");
			return -WM_FAIL;
		}

	} else {
		msg = MESSAGE(EVENT_WPS, event);
		err = os_queue_send(&provisioning_queue, &msg, OS_NO_WAIT);
		if (err != WM_SUCCESS) {
			prov_e("prov_wps_callback: Error in sending message");
		}
	}

	return WM_SUCCESS;
}

static struct wps_config wps_conf = {
	.role = 1,
	.pin_generator = 1,
	.version = 0x20,
	.version2 = 0x20,
	.device_name = "Marvell-WM",
	.manufacture = "Marvell",
	.model_name = "MC200-878x",
	.model_number = "0001",
	.serial_number = "0001",
	.config_methods = 0x2388,
	.primary_dev_category = 01,
	.primary_dev_subcategory = 01,
	.rf_bands = 1,
	.os_version = 0xFFFFFFFF,
	.wps_msg_max_retry = 5,
	.wps_msg_timeout = 5000,
	.pin_len = 8,
	.wps_callback = prov_wps_callback,
};
#endif

int verify_scan_interval_value(int scan_interval)
{
	if (scan_interval >= 2 && scan_interval <= 60)
		return WM_SUCCESS;
	return -WM_FAIL;
}

int prov_get_scan_config(struct prov_scan_config *scan_cfg)
{
	get_scan_params(&(scan_cfg->wifi_scan_params));
	scan_cfg->scan_interval = prov_g.scan_interval;
	return WM_SUCCESS;
}

int prov_set_scan_config_no_cb(struct prov_scan_config *scan_cfg)
{
	set_scan_params(&(scan_cfg->wifi_scan_params));
	if (!verify_scan_interval_value(scan_cfg->scan_interval))
		prov_g.scan_interval = scan_cfg->scan_interval;

	return WM_SUCCESS;
}

int prov_set_scan_config(struct prov_scan_config *scan_cfg)
{
	if (!prov_g.pc)
		return -WM_E_PROV_INVALID_STATE;

	(prov_g.pc)->provisioning_event_handler(PROV_SCAN_CFG_SET_CB,
						(void *)scan_cfg,
						sizeof(struct
						       prov_scan_config));

	return prov_set_scan_config_no_cb(scan_cfg);
}

int prov_set_network(int prov_method, struct wlan_network *net)
{
	int err;
	unsigned long msg;

	if (!prov_g.pc)
		return -WM_E_PROV_INVALID_STATE;

	err = prov_get_state();
	if (err == PROVISIONING_STATE_SET_CONFIG)
		return -WM_E_PROV_INVALID_STATE;
	memcpy(&(prov_g.current_net), net, sizeof(struct wlan_network));

	/* If provisioning state is PROVISIONING_STATE_DONE, provisioning is
	   successful and provisioning thread is not running. We still need to
	   send a callback to app that is waiting for set_network event. This
	   is required for /sys/network set requests that come post provisioning
	   */
	if (prov_g.state != PROVISIONING_STATE_DONE) {
		err = os_semaphore_get(&prov_g.sem_current_net, OS_NO_WAIT);
		if (err != WM_SUCCESS) {
			prov_w("set_network attempt already in progress");
			return -WM_E_PROV_INVALID_STATE;
		}

		if (prov_method == PROVISIONING_WLANNW)
			msg = MESSAGE(EVENT_CMD, SET_NW_WLANNW);
#ifdef CONFIG_WPS2
		else if (prov_method == PROVISIONING_WPS)
			msg = MESSAGE(EVENT_CMD, SET_NW_WPS);
#endif
		err = os_queue_send(&provisioning_queue, &msg, OS_NO_WAIT);
	} else {
		err = (prov_g.pc)->provisioning_event_handler
					(PROV_NETWORK_SET_CB,
					&prov_g.current_net,
					sizeof(struct wlan_network));
	}

	return err;
}

/* WLAN Connection Manager scan results callback */
static int prov_handle_scan_results(unsigned int count)
{
	int i, j = 0;
#ifdef CONFIG_WPS2
	int wps_session = 0;
	int index;
	unsigned long msg;
#endif
	/* WLAN firmware does not provide Noise Floor (NF) value with
	 * every scan result. We call the function below to update a
	 * global NF value so that /sys/scan can provide the latest NF value */

	wlan_get_current_nf();

	/* lock the scan results */
	if (os_mutex_get(&survey.lock, OS_WAIT_FOREVER) != WM_SUCCESS)
		return 0;

	/* clear out and refresh the site survey table */
	memset(survey.sites, 0, sizeof(survey.sites));
	/*
	 * Loop through till we have results to process or
	 * run out of space in survey.sites
	 */
	for (i = 0; i < count && j < MAX_SITES; i++) {
		if (wlan_get_scan_result(i, &survey.sites[j]))
			break;
#ifdef CONFIG_WPS2
		/* Check if provisioning state is interested in knowing SSIDs
		   that have PIN or PBC session active. If found we need to
		   ask application to select the SSID to attempt the session
		   with */

		if (prov_g.wps_state & PROV_WPS_PBC_ENABLED &&
		    survey.sites[j].wps_session == WPS_SESSION_PBC) {
			if (wps_session & 1)
				goto out;
			else
				wps_session = 1;
		}

		if (prov_g.wps_state & PROV_WPS_PIN_ENABLED &&
		    survey.sites[j].wps_session == WPS_SESSION_PIN &&
		    !(wps_session & 2))
			wps_session = 2;
#endif

		if (survey.sites[j].ssid[0] != 0)
			j++;
	}
	survey.num_sites = j;
#ifdef CONFIG_WPS2
	if (wps_session) {
		if (prov_g.state != PROVISIONING_STATE_ACTIVE)
			goto out;

		index =
		    (prov_g.
		     pc)->provisioning_event_handler(PROV_WPS_SSID_SELECT_REQ,
						     &(survey.sites),
						     sizeof(survey.sites));
		if (index == -1)
			goto out;

		if (index >= MAX_SITES)
			goto out;

		if (survey.sites[index].wps == 0)
			goto out;

		if ((prov_g.wps_state & PROV_WPS_PBC_ENABLED &&
		     survey.sites[index].wps_session == WPS_SESSION_PBC) ||
		    (prov_g.wps_state & PROV_WPS_PIN_ENABLED &&
		     survey.sites[index].wps_session == WPS_SESSION_PIN)) {
			memcpy(&(prov_g.wps_session_nw), &(survey.sites[index]),
			       sizeof(struct wlan_scan_result));
			msg = MESSAGE(EVENT_CMD, WPS_SESSION_ATTEMPT);
			os_queue_send(&provisioning_queue, &msg, OS_NO_WAIT);
		}
	}
 out:
#endif

	/* unlock the site survey table */
	os_mutex_put(&survey.lock);
	/* Give up the semaphore to process next scan/prov_stop request */
	os_semaphore_put(&prov_scan_protection_sem);
	return 0;
}


#ifdef CONFIG_WPS2
static void wps_prov_init(unsigned long event, unsigned long data)
{
	prov_d("wps_prov_init event %d data %d", event, data);

	if ((event == EVENT_CMD) && (data == PROV_INIT_SM)) {
		prov_d("WPS started");
		wps_start(&wps_conf);
	}

	if ((event == EVENT_WPS) && (data == WPS_STARTED)) {
		prov_g.wps_state |= PROV_WPS_STARTED;
	}

	return;
}
#endif

static void switch_to_set_config()
{
	prov_g.state = PROVISIONING_STATE_SET_CONFIG;
	prov_g.scan_running = 0;

	return;
}

static void state_machine_wlannw(unsigned long event, unsigned long data)
{
	prov_d("state_machine_wlannw event %d data %d", event, data);

	if (event == EVENT_CMD && data == SET_NW_WLANNW) {
		switch_to_set_config();
		prov_g.prov_nw_state = PROV_WLANNW_SUCCESSFUL;
	}

	return;
}

#ifdef CONFIG_WPS2
static void state_machine_wps(unsigned long event, unsigned long data)
{
	int ret;
	enum wps_session_command wps_cmd;
	uint32_t wps_pin;
	struct wlan_scan_result wps_res;

	prov_d("state_machine_wps event %d data %d", event, data);

	if (event == EVENT_CMD && data == SET_NW_WPS) {
		prov_g.wps_state = PROV_WPS_SUCCESSFUL;
		switch_to_set_config();
	}

	if (event == EVENT_CMD && data == WPS_SESSION_ATTEMPT) {
		if (prov_g.wps_state & PROV_WPS_PBC_ENABLED) {
			wps_cmd = CMD_WPS_PBC;
			prov_g.time_pbc_expiry = 0;
			wps_pin = 0;
			prov_g.wps_state &= ~(PROV_WPS_PBC_ENABLED);
		} else if (prov_g.wps_state & PROV_WPS_PIN_ENABLED) {
			wps_cmd = CMD_WPS_PIN;
			prov_g.time_pin_expiry = 0;
			wps_pin = prov_g.wps_pin;
			prov_g.wps_pin = 0;
			prov_g.wps_state &= ~(PROV_WPS_PIN_ENABLED);
		} else {
			return;
		}

		memcpy(&(wps_res), &(prov_g.wps_session_nw),
		       sizeof(struct wlan_scan_result));

		memset(&(prov_g.wps_session_nw), 0,
		       sizeof(struct wlan_scan_result));

		ret = wps_connect(wps_cmd, wps_pin, &wps_res);
		if (ret != WM_SUCCESS) {
			prov_e("wps_send_command failed");
		}
	}

	if (event == EVENT_WPS) {
		switch (data) {
		case WPS_SESSION_STARTED:
			(prov_g.pc)->provisioning_event_handler
			    (PROV_WPS_SESSION_STARTED, NULL, 0);
			break;
		case WPS_SESSION_SUCCESSFUL:
			is_wps_requested = false;
			break;
		case WPS_SESSION_TIMEOUT:
		case WPS_SESSION_ABORTED:
		case WPS_SESSION_FAILED:
			(prov_g.pc)->provisioning_event_handler
			    (PROV_WPS_SESSION_UNSUCCESSFUL, NULL, 0);
			/* Resetting prov_g variables to initial provisioning
			 * state so that wps session can be started again
			 */
			prov_g.prov_nw_state = PROV_WLANNW_STARTED;
			prov_g.state = PROVISIONING_STATE_ACTIVE;
			prov_g.scan_running = 1;
			prov_g.wps_state = PROV_WPS_STARTED;
			is_wps_requested = false;
			break;
		default:
			break;
		}
	}

	return;
}

static void handle_pin_pbc_expiry(void)
{
	unsigned long cur_time;

	cur_time = os_ticks_get();

	if (prov_g.time_pbc_expiry && cur_time >= prov_g.time_pbc_expiry) {
		prov_g.time_pbc_expiry = 0;
		prov_g.wps_state &= ~(PROV_WPS_PBC_ENABLED);
		(prov_g.pc)->provisioning_event_handler(PROV_WPS_PBC_TIMEOUT,
							NULL, 0);
	}

	if (prov_g.time_pin_expiry && cur_time >= prov_g.time_pin_expiry) {
		prov_g.time_pin_expiry = 0;
		prov_g.wps_state &= ~(PROV_WPS_PIN_ENABLED);
		prov_g.wps_pin = 0;
		(prov_g.pc)->provisioning_event_handler(PROV_WPS_PIN_TIMEOUT,
							NULL, 0);
	}

	return;
}
#endif

int prov_set_scan_policy(enum prov_scan_policy sp)
{
	/* Checking if the scan policy specified is valid */
	if (sp < 0 || sp > 2) {
		prov_d("Invalid scan policy specified");
		return -WM_FAIL;
	}

	scan_policy = sp;
	/* Create a semaphore for aperiodic scan */
	if (scan_policy == PROV_ON_DEMAND_SCAN) {
		if (!is_sem_created) {
			int ret = os_semaphore_create(&os_sem_on_demand_scan,
						      "sem_on_demand_scan");
			if (ret) {
				prov_d("Failed to create on demand scan"
				       " semaphore: %d", ret);
				return -WM_FAIL;
			}
			/* Since initial scanning is already done */
			os_semaphore_get(&os_sem_on_demand_scan, OS_NO_WAIT);
			is_sem_created = 1;
		}
	}
	return WM_SUCCESS;
}

static int prov_initiate_scan(unsigned long last_scan_time,
			      unsigned long scan_ticks)
{
	/* Get the scan-config for periodic/aperiodic scan */
	if (scan_policy == PROV_PERIODIC_SCAN || is_wps_requested) {
		if (((os_ticks_get() - last_scan_time) >= scan_ticks)) {
			return 1;
		}
	} else if ((scan_policy == PROV_ON_DEMAND_SCAN) && is_sem_created) {
		if (os_semaphore_get(&os_sem_on_demand_scan, OS_NO_WAIT)
			== WM_SUCCESS) {
			return 1;
		}
	}
	return 0;
}

static void prov_state_machine(os_thread_arg_t unused)
{
	unsigned long msg, event, data;
	int ret;
	unsigned long last_scan_time = 0, scan_ticks;

	prov_g.scan_running = 0;

	(prov_g.pc)->provisioning_event_handler(PROV_START, NULL, 0);

	prov_d("prov_state_machine started. scan interval = %d",
	    prov_g.scan_interval);

	prov_g.prov_nw_state = PROV_WLANNW_STARTED;
	prov_g.state = PROVISIONING_STATE_INITIALIZING;

	/* Creating the semaphore for on demand scanning if not created */
	if (scan_policy == PROV_ON_DEMAND_SCAN) {
		if (!is_sem_created) {
			int ret = os_semaphore_create(&os_sem_on_demand_scan,
						      "sem_on_demand_scan");
			if (ret) {
				prov_d("Failed to create on demand scan"
				       " semaphore: %d", ret);
			}
			/* Since initial scanning is already done */
			os_semaphore_get(&os_sem_on_demand_scan, OS_NO_WAIT);
			is_sem_created = 1;
		}
	}

	/* Start the provisioning state machine */
	msg = MESSAGE(EVENT_CMD, PROV_INIT_SM);
	ret = os_queue_send(&provisioning_queue, &msg,
			      OS_NO_WAIT);
	if (ret != WM_SUCCESS) {
		prov_e("Error in sending message");
	}

	/* Perform the first scan */
	wlan_scan(prov_handle_scan_results);

	while (1) {
		/* Get message from queue. In case of timeout, run wlan_scan if
		   scan should be running */
		scan_ticks = os_msec_to_ticks(prov_g.scan_interval * 1000);
		ret = os_queue_recv(&provisioning_queue, &msg, scan_ticks);
		event = MESSAGE_EVENT(msg);
		data = MESSAGE_DATA(msg);

		prov_d("Provisioning SM: event = %d data = %d", event, data);
		if (event == EVENT_CMD && data == PROV_STOP) {
			break;
		}

		if (prov_initiate_scan(last_scan_time, scan_ticks)
		    && prov_g.scan_running) {
			last_scan_time = os_ticks_get();
			ret = os_semaphore_get(&prov_scan_protection_sem,
					       os_msec_to_ticks(
					       DEFAULT_SCAN_PROTECTION_TIMEOUT
					       * 1000));
			if (ret != WM_SUCCESS)
				prov_e("Scan protection timeout occurred."
				       " Failed to get the scan protection"
				       " semaphore.", ret);
			wlan_scan(prov_handle_scan_results);
		}
#ifdef CONFIG_WPS2
		handle_pin_pbc_expiry();
#endif
		if (ret != WM_SUCCESS)
			continue;

#ifdef CONFIG_WPS2
		/* Check if WPS provisioning is enabled. If enabled, check
		   if initialization is required */
		if ((prov_g.pc)->prov_mode & PROVISIONING_WPS) {
			if (!(prov_g.wps_state & PROV_WPS_STARTED)) {
				prov_g.scan_running = 0;
				prov_g.state = PROVISIONING_STATE_INITIALIZING;
				wps_prov_init(event, data);
			}
		} else {
			prov_g.wps_state = PROV_WPS_STARTED;
		}
#endif

		if ((prov_g.state == PROVISIONING_STATE_INITIALIZING) &&
#ifdef CONFIG_WPS2
		    (prov_g.wps_state & PROV_WPS_STARTED) &&
#endif
		    (prov_g.prov_nw_state & PROV_WLANNW_STARTED)) {

			prov_g.scan_running = 1;
			prov_g.state = PROVISIONING_STATE_ACTIVE;
		}

		if (prov_g.state == PROVISIONING_STATE_ACTIVE ||
		    prov_g.state == PROVISIONING_STATE_SET_CONFIG) {
			if ((prov_g.pc)->prov_mode & PROVISIONING_WLANNW) {
				state_machine_wlannw(event, data);
			}
#ifdef CONFIG_WPS2
			if ((prov_g.pc)->prov_mode & PROVISIONING_WPS) {
				state_machine_wps(event, data);
			}
#endif

#ifndef CONFIG_WPS2
			if (prov_g.prov_nw_state & PROV_WLANNW_SUCCESSFUL) {
#else
			if ((prov_g.prov_nw_state & PROV_WLANNW_SUCCESSFUL) ||
			    (prov_g.wps_state & PROV_WPS_SUCCESSFUL)) {
				if (prov_g.wps_state & PROV_WPS_SUCCESSFUL) {
					(prov_g.pc)->provisioning_event_handler(
						PROV_WPS_SESSION_SUCCESSFUL,
						NULL, 0);
				}
#endif
				(prov_g.pc)->provisioning_event_handler(
					PROV_NETWORK_SET_CB,
					&prov_g.current_net,
					sizeof(struct wlan_network));
				prov_g.scan_running = 0;
				prov_g.state = PROVISIONING_STATE_DONE;
				ret = os_semaphore_put(&prov_g.sem_current_net);
				/* The Provisioning Module should only stop its
				   operation if application explicitly issues
				   the PROV_STOP command */
				continue;
			}
		}
	}
	if (scan_policy == PROV_ON_DEMAND_SCAN) {
		os_semaphore_delete(&os_sem_on_demand_scan);
		is_sem_created = 0;
	}
	/* Below get on semaphore is to ensure that any
	   outstanding wlan_scan callback has already
	   been called and there is no pending scan
	   request futher */
	ret = os_semaphore_get(&prov_scan_protection_sem, os_msec_to_ticks(
				       DEFAULT_SCAN_PROTECTION_TIMEOUT * 1000));
	if (ret != WM_SUCCESS)
		prov_e("Scan protection timeout occurred. Failed to get the "
		       "scan protection semaphore.", ret);
	prov_g.state = PROVISIONING_STATE_STOP_REQUESTED;
	if ((prov_g.pc)->prov_mode != PROVISIONING_EZCONNECT) {
		os_mutex_put(&survey.lock);
		os_mutex_delete(&survey.lock);
		/* We are not deleting sem_current_net
		   intentionally because it will be used
		   by subsequent set_network requests */
	}
	os_queue_delete(&provisioning_queue);
	prov_variable_reset();
	prov_g.state = PROVISIONING_STATE_STOPPED;
	os_semaphore_delete(&prov_scan_protection_sem);
	os_thread_delete(NULL);
}

extern void prov_ezconnect(os_thread_arg_t unused);
extern void prov_ezconnect_stop();

int prov_start(struct provisioning_config *pc)
{
	int ret;

	if (prov_g.pc != NULL) {
		prov_w("already configured!");
		return -WM_E_PROV_INVALID_CONF;
	}

	if (pc == NULL) {
		prov_w("configuration not provided!");
		return -WM_E_PROV_INVALID_CONF;
	}

	if (!pc->provisioning_event_handler) {
		prov_w("No event handler specified");
		return -WM_E_PROV_INVALID_CONF;
	}

	prov_g.pc = pc;

	ret = os_queue_create(&provisioning_queue, "p_queue", sizeof(void *),
			      &provisioning_queue_data);
	if (ret)
		goto fail;

	memset(&survey, 0, sizeof(survey));

	ret = os_mutex_create(&survey.lock, "site-survey",
			      OS_MUTEX_INHERIT);
	if (ret)
		goto fail;

	ret = os_semaphore_create(&prov_scan_protection_sem,
				  "prov_scan_protection_sem");
	if (ret)
		goto fail;

	if (!prov_g.sem_current_net) {
		ret = os_semaphore_create(&prov_g.sem_current_net,
					  "current_net_sem");
		if (ret) {
			prov_e("Failed to create current net semaphore"
			       ": %d", ret);
			goto fail;
		}
	}

	ret = os_thread_create(&provisioning_thread,
			       "provisioning_demo",
			       prov_state_machine,
			       0,
			       &provisioning_stack,
			       OS_PRIO_3);
	if (ret) {
		prov_e("Failed to launch thread: %d", ret);
		goto fail;
	}

	return 0;

 fail:
	if (provisioning_thread)
		os_thread_delete(&provisioning_thread);
	if (survey.lock)
		os_mutex_delete(&survey.lock);
	if (prov_g.sem_current_net)
		os_semaphore_delete(&prov_g.sem_current_net);
	if (provisioning_queue)
		os_queue_delete(&provisioning_queue);
	if (prov_scan_protection_sem)
		os_semaphore_delete(&prov_scan_protection_sem);

	return -WM_FAIL;
}

int prov_ezconnect_start(struct provisioning_config *pc)
{
	int ret;

	if (prov_g.pc != NULL) {
		prov_w("already configured!");
		return -WM_E_PROV_INVALID_CONF;
	}

	if (pc == NULL) {
		prov_w("configuration not provided!");
		return -WM_E_PROV_INVALID_CONF;
	}

	if (!pc->provisioning_event_handler) {
		prov_w("No event handler specified");
		return -WM_E_PROV_INVALID_CONF;
	}

	prov_g.pc = pc;

	ret = os_thread_create(&provisioning_thread,
			       "ezconnect_smc",
			       prov_ezconnect,
			       (void *) &provisioning_thread,
			       &provisioning_stack,
			       OS_PRIO_3);
	if (ret) {
		prov_e("Failed to launch thread: %d", ret);
		goto fail;
	}
	return 0;

 fail:
	if (provisioning_thread)
		os_thread_delete(&provisioning_thread);

	return -WM_FAIL;
}

void prov_ezconnect_finish(void)
{
	int ret;

	prov_ezconnect_stop();

	if (((prov_g.pc)->prov_mode & PROVISIONING_EZCONNECT) &&
	    provisioning_thread) {
		ret = os_thread_delete(&provisioning_thread);
		if (ret != WM_SUCCESS)
			prov_w("failed to delete thread: %d", ret);
		provisioning_thread = NULL;
	}

	prov_variable_reset();
}

void prov_finish(void)
{
	int ret;
	unsigned long msg;
	/*
	 * if this fails, it's probably because the thread quit on its own.
	 * tx_thread_terminate(&provisioning_thread);
	 */
#ifdef CONFIG_WPS2
	if ((prov_g.pc)->prov_mode & PROVISIONING_WPS) {
		prov_d("Stopping wps thread");
		wps_stop();
	}
#endif
	if (prov_g.state == PROVISIONING_STATE_STOPPED ||
	    prov_g.state == PROVISIONING_STATE_STOP_REQUESTED)
		return;

	msg = MESSAGE(EVENT_CMD, PROV_STOP);
	ret = os_queue_send(&provisioning_queue, &msg, OS_NO_WAIT);
	if (ret != WM_SUCCESS)
		prov_e("Error in sending message");
	while (!(prov_g.state == PROVISIONING_STATE_STOPPED)) {
		os_thread_sleep(os_msec_to_ticks(10));
	}
}


int prov_get_lock_scan_results()
{
	if (!(prov_g.state == PROVISIONING_STATE_STOPPED ||
	      prov_g.state == PROVISIONING_STATE_STOP_REQUESTED)) {
		if (os_mutex_get(&survey.lock, OS_WAIT_FOREVER) != WM_SUCCESS)
			return -WM_FAIL;
		else
			return WM_SUCCESS;
	}
	return -WM_FAIL;
}

void prov_put_lock_scan_results()
{
	if (!(prov_g.state == PROVISIONING_STATE_STOPPED ||
	      prov_g.state == PROVISIONING_STATE_STOP_REQUESTED)) {
		os_mutex_put(&survey.lock);
	}
}

int prov_get_scan_count()
{
	int i;

	if (!(prov_g.state == PROVISIONING_STATE_STOPPED ||
	      prov_g.state == PROVISIONING_STATE_STOP_REQUESTED)) {
		for (i = survey.num_sites - 1; i >= 0; i--) {
			if (survey.sites[i].ssid[0] == 0)
				survey.num_sites--;
			else
				/* Found valid entry, exit */
				break;
		}
		return survey.num_sites;
	}
	return -WM_FAIL;
}

int prov_get_scan_result(struct wlan_scan_result **sptr, unsigned int index)
{

	if (!(prov_g.state == PROVISIONING_STATE_STOPPED ||
	      prov_g.state == PROVISIONING_STATE_STOP_REQUESTED)) {
		if (index > survey.num_sites)
			return -WM_FAIL;

		*sptr = &(survey.sites[index]);

		return WM_SUCCESS;
	}
	return -WM_FAIL;
}
