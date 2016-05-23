/*
 * Copyright 2008-2015, Marvell International Ltd.
 * All Rights Reserved.
 *
 */

/** provisioning_web_handlers.c: Web Handlers for the provisioning interface
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <wmtypes.h>
#include <wmstdio.h>
#include <wm_os.h>
#include <wlan.h>
#include <httpd.h>
#include <http_parse.h>
#include <http-strings.h>
#include <cli_utils.h>		/* for get_mac() */
#include <provisioning.h>
#include <wm_net.h>
#include <json_parser.h>
#include <json_generator.h>

#include "provisioning_int.h"


/*
 * Buffer to parse JSON values from /sys/network POST response, maximum size
 * is MAX_JSON_VAL_LEN(128) + 1 (null termination).
 */
static char json_val[MAX_JSON_VAL_LEN + 1];
static struct json_str jstr;
extern enum prov_scan_policy scan_policy;
extern os_semaphore_t os_sem_on_demand_scan;

int (*network_set_cb) (struct wlan_network *network) = NULL;
static struct wlan_network current_net;

/*It is assumed that ip address(unsigned ip) will be in network order*/
static inline int validate_ip(unsigned ip)
{
	/* This will return -1 if one of the following case is true
	   1. x.x.x.255
	   2. x.x.x.0
	   3  0.x.x.x
	   4. 255.x.x.x
	   There is no further need to validate 0.0.0.0 and 255.255.255.255
	 */
	if (((ip >> 24) == 0XFF) ||
	    !(ip >> 24) || ((ip & 0xFF) == 0XFF) || !(ip & 0XFF))
		return -1;
	return 0;
}

static const char *parse_network_settings(jobj_t *jobj,
			     struct wlan_network *network, int prov_mode)
{
	int security, channel, ret;
	char *tag = NULL;
	unsigned int len = 0;
	const char *errormsg = NULL;
	long addr;
	int json_int_val;

	memset(network, 0, sizeof(struct wlan_network));
	if (json_get_val_str(jobj, J_NAME_SSID, json_val, sizeof(json_val)) !=
	    WM_SUCCESS)
		return invalid_json_element;

	if (strlen(json_val) > IEEEtypes_SSID_SIZE || strlen(json_val) <= 0)
		return invalid_ssid;

	strncpy(network->ssid, json_val, IEEEtypes_SSID_SIZE);

	ret = json_get_val_int(jobj, J_NAME_CHANNEL, &channel);
	if (ret == WM_SUCCESS)
		network->channel = channel;
	else
		network->channel = 0;

	if (json_get_val_int(jobj, J_NAME_SECURITY, &security) != WM_SUCCESS)
		return invalid_json_element;

	if (is_valid_security(security) == -1)
		return invalid_security;

	network->security.type = (enum wlan_security_type)security;

	if ((network->security.type == WLAN_SECURITY_WEP_OPEN) ||
		(network->security.type == WLAN_SECURITY_WPA) ||
		(network->security.type == WLAN_SECURITY_WPA2) ||
		(network->security.type == WLAN_SECURITY_WPA_WPA2_MIXED) ||
		(network->security.type == WLAN_SECURITY_WILDCARD)) {
		ret = json_get_val_str(jobj, J_NAME_KEY, json_val,
				       sizeof(json_val));
		if (ret != WM_SUCCESS && (network->security.type
					  != WLAN_SECURITY_WILDCARD))
			return invalid_key;
		if (ret == WM_SUCCESS) {
			tag = json_val;
			len = strlen(tag);
		}
	}

	if (tag) {
		/* be sure to copy the null */
		strncpy(network->security.psk, tag, len + 1);
		network->security.psk_len = len;
	}
	if (json_get_val_int(jobj, J_NAME_IPTYPE, &json_int_val) != WM_SUCCESS)
		json_int_val = ADDR_TYPE_DHCP;	/* Default is dhcp */

	if (json_int_val == ADDR_TYPE_STATIC) {
		/* assume failure */
		network->ip.ipv4.addr_type = ADDR_TYPE_STATIC;
		errormsg = invalid_ip_settings;
		PARSE_IP_ELEMENT(jobj, J_NAME_IP, network->ip.ipv4.address);
		if (validate_ip(network->ip.ipv4.address) != WM_SUCCESS)
			goto done;
		PARSE_IP_ELEMENT(jobj, J_NAME_GW, network->ip.ipv4.gw);
		if (validate_ip(network->ip.ipv4.gw) != WM_SUCCESS)
			goto done;
		PARSE_IP_ELEMENT(jobj, J_NAME_NETMASK,
				network->ip.ipv4.netmask);
		if (json_get_val_str(jobj, J_NAME_DNS1, json_val,
				     sizeof(json_val)) == WM_SUCCESS) {
			network->ip.ipv4.dns1 = inet_addr(json_val);
		}
		if (json_get_val_str(jobj, J_NAME_DNS2, json_val,
				     sizeof(json_val)) == WM_SUCCESS) {
			network->ip.ipv4.dns2 = inet_addr(json_val);
		}
		errormsg = NULL;
	} else if (json_int_val == ADDR_TYPE_LLA) {
		network->ip.ipv4.addr_type = ADDR_TYPE_LLA;

	} else {
		network->ip.ipv4.addr_type = ADDR_TYPE_DHCP;
	}

 done:
	return errormsg;
}

static int set_network_handler(httpd_request_t *req)
{
	const int size = 512;
	char sys_nw_resp[128];
	const char *errormsg;
	int err = -1, ret;
	char *sys_nw_req;
	int provisioning_mode;

	sys_nw_req = os_mem_calloc(size);
	if (!sys_nw_req) {
		prov_e("Failed to allocate response content");
		return -WM_E_NOMEM;
	}

	err = httpd_get_data(req, sys_nw_req, size);
	if (err < 0) {
		prov_e("Failed to get post request data");
		goto out1;
	}
	/* Parse the JSON data */
	jsontok_t json_tokens[NUM_TOKENS];
	jobj_t jobj;
	ret = json_init(&jobj, json_tokens, NUM_TOKENS,
			sys_nw_req, req->body_nbytes);
	if (ret != WM_SUCCESS)
		return -WM_FAIL;
	provisioning_mode = prov_get_state();

	if (provisioning_mode == PROVISIONING_STATE_SET_CONFIG) {
		snprintf(sys_nw_resp, sizeof(sys_nw_resp), HTTPD_JSON_ERR_MSG,
			failed_to_set_network);
		err = -WM_FAIL;
		goto out;
	}

	errormsg = parse_network_settings(&jobj, &current_net,
					  provisioning_mode);
	if (errormsg) {
		snprintf(sys_nw_resp, sizeof(sys_nw_resp),
				HTTPD_JSON_ERR_MSG, errormsg);
		err = -WM_FAIL;
		goto out;
	}
	/* Send response before setting the network */
	ret = httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_SUCCESS,
		strlen(HTTPD_JSON_SUCCESS), HTTP_CONTENT_JSON_STR);

	/* Wait on http thread to send response */
	os_thread_sleep(os_msec_to_ticks(1000));

	if (provisioning_mode != PROVISIONING_STATE_ACTIVE) {
		if (network_set_cb)
			err = (*network_set_cb)(&current_net);
	} else {
		err = prov_set_network(PROVISIONING_WLANNW, &current_net);
	}
	os_mem_free(sys_nw_req);
	return ret;

out:
	err = httpd_send_response(req, HTTP_RES_200, sys_nw_resp,
		strlen(sys_nw_resp), HTTP_CONTENT_JSON_STR);
out1:
	os_mem_free(sys_nw_req);

	return err;
}

static int get_network_handler(httpd_request_t *req)
{
	int ret = WM_SUCCESS;
	const int size = 512;
	char *content;
	struct wlan_network network;
	struct in_addr ip;
	char tempstr[50] = { 0, };
	short rssi;

	content = os_mem_calloc(size);

	if (!content) {
		prov_e("Failed to allocate memory\r\n");
		return -WM_E_NOMEM;
	}

	if (prov_get_state() == PROVISIONING_STATE_ACTIVE) {
		if (wlan_get_current_uap_network(&network)) {
			ret = -WM_FAIL;
			goto out;
		}
	} else {
		if (wlan_get_current_network(&network)) {
			ret = -WM_FAIL;
			goto out;
		}
	}

	wlan_get_current_rssi(&rssi);

	json_str_init(&jstr, content, size);
	json_start_object(&jstr);

	json_set_val_str(&jstr, J_NAME_SSID, network.ssid);

	snprintf(tempstr, sizeof(tempstr), "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
		 network.bssid[0], network.bssid[1], network.bssid[2],
		 network.bssid[3], network.bssid[4], network.bssid[5]);

	json_set_val_str(&jstr, J_NAME_BSSID, tempstr);
	json_set_val_int(&jstr, J_NAME_CHANNEL, network.channel);
	json_set_val_int(&jstr, J_NAME_SECURITY, network.security.type);
	json_set_val_int(&jstr, J_NAME_RSSI, rssi);

	json_push_object(&jstr, J_NAME_IP);

	if (is_sta_ipv4_connected()) {
		json_push_object(&jstr, J_NAME_IPV4);
		json_set_val_int(&jstr, J_NAME_IPTYPE,
			network.ip.ipv4.addr_type);
		ip.s_addr = network.ip.ipv4.address;
		json_set_val_str(&jstr, J_NAME_IP_ADDR, inet_ntoa(ip));
		ip.s_addr = network.ip.ipv4.netmask;
		json_set_val_str(&jstr, J_NAME_NETMASK, inet_ntoa(ip));
		ip.s_addr = network.ip.ipv4.gw;
		json_set_val_str(&jstr, J_NAME_GW, inet_ntoa(ip));
		ip.s_addr = network.ip.ipv4.dns1;
		json_set_val_str(&jstr, J_NAME_DNS1, inet_ntoa(ip));
		ip.s_addr = network.ip.ipv4.dns2;
		json_set_val_str(&jstr, J_NAME_DNS2, inet_ntoa(ip));
		json_pop_object(&jstr); /* J_NAME_IPV4 */
	}

#ifdef CONFIG_IPV6
	if (is_sta_ipv6_connected()) {
		int i;
		json_push_array_object(&jstr, J_NAME_IPV6);
		for (i = 0; i < MAX_IPV6_ADDRESSES; i++) {
			if (network.ip.ipv6[i].addr_state != IP6_ADDR_INVALID) {
				json_start_object(&jstr);
				json_set_val_str(&jstr, J_NAME_SCOPE,
				ipv6_addr_type_to_desc(&network.ip.ipv6[i]));
				json_set_val_str(&jstr, J_NAME_IP_ADDR,
				inet6_ntoa((network.ip.ipv6[i].address)));
				json_close_object(&jstr);
			}
		}
		json_pop_array_object(&jstr); /* J_NAME_IPV6 */
	}
#endif
	json_pop_object(&jstr); /* J_NAME_IP */
	json_close_object(&jstr);
	ret = httpd_send_response(req, HTTP_RES_200, content, strlen(content),
		HTTP_CONTENT_JSON_STR);
out:
	os_mem_free(content);
	return ret;
}

static struct httpd_wsgi_call http_handle_network = {"/sys/network",
		HTTPD_DEFAULT_HDR_FLAGS, 0, get_network_handler,
		set_network_handler, NULL, NULL};

static const char *parse_scan_config(jobj_t *jobj,
			struct prov_scan_config *scan_cfg)
{
	const char *errormsg = NULL;
	int json_int_val;
	int change_requested = 0;

	if (!json_get_val_int(jobj, J_NAME_SCAN_DURATION, &json_int_val)) {
		if (json_int_val < 50 || json_int_val > 500)
			errormsg = invalid_scan_duration;
		else
			(scan_cfg->wifi_scan_params).scan_duration =
			    json_int_val;
		change_requested = 1;
	}
	if (!json_get_val_int(jobj, J_NAME_SPLIT_SCAN_DELAY, &json_int_val)) {
		if (json_int_val < 30 || json_int_val > 300)
			errormsg = invalid_split_scan_delay;
		else
			(scan_cfg->wifi_scan_params).split_scan_delay =
				json_int_val;
		change_requested = 1;
	}
	if (!json_get_val_int(jobj, J_NAME_SCAN_INTERVAL, &json_int_val)) {
		if (json_int_val < 2 || json_int_val > 60)
			errormsg = invalid_scan_interval;
		else
			scan_cfg->scan_interval = json_int_val;
		change_requested = 1;
	}

	if (!json_get_array_object(jobj, J_NAME_CHANNEL, NULL)) {
		if (json_array_get_int(jobj, 0, &json_int_val) != WM_SUCCESS) {
			errormsg = invalid_channel_list;
		} else {
			if (json_int_val > 11 || json_int_val < 0)
				errormsg = invalid_channel_number;
			else
				(scan_cfg->wifi_scan_params).channel[0] =
				    json_int_val;
		}
		change_requested = 1;
	}

	/* If there is no matching element in the request */
	if (errormsg == NULL && change_requested == 0)
		errormsg = no_valid_element;

	return errormsg;
}

static int set_scan_config_handler(httpd_request_t *req)
{
	const int size = 128;
	const char *errormsg = NULL;
	int err;
	char *scan_cfg_resp;
	char scan_cfg_req[100];
	struct prov_scan_config scan_cfg;

	scan_cfg_resp = os_mem_calloc(size);
	if (!scan_cfg_resp) {
		prov_e("Failed to allocate response content");
		return -WM_E_NOMEM;
	}

	err = httpd_get_data(req, scan_cfg_req,
			sizeof(scan_cfg_req));
	if (err < 0) {
		prov_e("Failed to get post request data");
		goto out;
	}
	/* Parse the JSON data */
	jsontok_t json_tokens[NUM_TOKENS];
	jobj_t jobj;
	err = json_init(&jobj, json_tokens, NUM_TOKENS,
			scan_cfg_req, req->body_nbytes);
	if (err != WM_SUCCESS)
		return -WM_FAIL;

	scan_cfg.wifi_scan_params.bssid = NULL;
	scan_cfg.wifi_scan_params.ssid = NULL;
	scan_cfg.wifi_scan_params.channel[0] = -1;
	scan_cfg.wifi_scan_params.bss_type = BSS_ANY;
	scan_cfg.wifi_scan_params.scan_duration = -1;
	scan_cfg.wifi_scan_params.split_scan_delay = -1;

	errormsg = parse_scan_config(&jobj, &scan_cfg);
	if (errormsg) {
		snprintf(scan_cfg_resp, size, HTTPD_JSON_ERR_MSG, errormsg);
		err = httpd_send_response(req, HTTP_RES_200, scan_cfg_resp,
			strlen(scan_cfg_resp), HTTP_CONTENT_JSON_STR);
		goto out;
	}
	err = prov_set_scan_config(&scan_cfg);
	if (err == WM_SUCCESS)
		err = httpd_send_response(req, HTTP_RES_200,
			HTTPD_JSON_SUCCESS, strlen(HTTPD_JSON_SUCCESS),
			HTTP_CONTENT_JSON_STR);
	else
		err = httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_ERROR,
			strlen(HTTPD_JSON_ERROR), HTTP_CONTENT_JSON_STR);
out:
	os_mem_free(scan_cfg_resp);
	return err;
}


static int get_scan_config_handler(httpd_request_t *req)
{
	const int size = 100;
	int ret;
	char *content;
	struct prov_scan_config scan_cfg;

	content = os_mem_calloc(size);

	if (!content) {
		prov_e("Failed to allocate memory\r\n");
		return -WM_E_NOMEM;
	}

	prov_get_scan_config(&scan_cfg);

	json_str_init(&jstr, content, size);
	json_start_object(&jstr);
	json_set_val_int(&jstr, J_NAME_SCAN_DURATION,
			 scan_cfg.wifi_scan_params.scan_duration);
	json_set_val_int(&jstr, J_NAME_SPLIT_SCAN_DELAY,
			scan_cfg.wifi_scan_params.split_scan_delay);
	json_set_val_int(&jstr, J_NAME_SCAN_INTERVAL, scan_cfg.scan_interval);
	json_push_array_object(&jstr, J_NAME_CHANNEL);
	json_set_array_int(&jstr, scan_cfg.wifi_scan_params.channel[0]);
	json_close_array(&jstr);
	json_close_object(&jstr);
	ret = httpd_send_response(req, HTTP_RES_200, content, strlen(content),
		HTTP_CONTENT_JSON_STR);
	os_mem_free(content);
	return ret;
}

static int wsgi_handler_get_scan_results(httpd_request_t *req)
{
	int i;
	const int size = 512;
	unsigned char count;
	struct json_str jstr, *jptr;
	int err = WM_SUCCESS, mode;
	char index_str[3];
	char tempstr[50] = { 0, };
	char *content;
	int bcn_nf_last;

	content = os_mem_calloc(size);

	if (!content) {
		prov_e("Failed to allocate memory\r\n");
		return -WM_E_NOMEM;
	}

	/* Invoke the server provided handlers to process HTTP headers */
	err = httpd_parse_hdr_tags(req, req->sock, content, size);
	if (err != WM_SUCCESS)
		goto out;

	/* Convenience string that sends a 200 OK status and sets the
	 * Transfer-Encoding to chunked.
	 */
	if (httpd_send(req->sock, http_header_200,
		       strlen(http_header_200)) != WM_SUCCESS) {
		err = -WM_FAIL;
		goto out;
	}

	if (httpd_send(req->sock, http_header_type_chunked,
		       strlen(http_header_type_chunked)) != WM_SUCCESS) {
		err = -WM_FAIL;
		goto out;
	}


	/* Set content type */
	if (httpd_send_str(req->sock,
			   (char *)&http_content_type_json_nocache[0]) !=
	    WM_SUCCESS) {
		err = -WM_FAIL;
		goto out;
	}

	if (os_mutex_get(&survey.lock, OS_WAIT_FOREVER) != WM_SUCCESS) {
		err = -WM_FAIL;
		goto out;
	}
	jptr = &jstr;

	/* Skip network which have NULL ssid from last, by this we make sure
	 * last entry in survey struct is non-null.
	 */
	for (i = survey.num_sites - 1; i >= 0; i--) {
		if (survey.sites[i].ssid[0] == 0)
			survey.num_sites--;
		else
			/* Found valid entry, exit */
			break;
	}

	bcn_nf_last = wlan_get_current_nf();

	json_str_init(jptr, content, size);
	json_start_object(jptr);
	json_push_array_object(jptr, J_NAME_NETWORKS);

	for (i = 0, count = 0; i < survey.num_sites; i++) {
		if (survey.sites[i].ssid[0] == 0)
			continue;
		else
			/* Found valid ssid increment count */
			count++;

		mode = 0;
		/* Our preference is in the following order: wpa2, wpa, wep */
		if (survey.sites[i].wpa2 && survey.sites[i].wpa)
			mode = 5;
		else if (survey.sites[i].wpa2)
			mode = 4;
		else if (survey.sites[i].wpa)
			mode = 3;
		else if (survey.sites[i].wep)
			mode = 1;
		snprintf(tempstr, sizeof(tempstr),
			 "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
			 survey.sites[i].bssid[0], survey.sites[i].bssid[1],
			 survey.sites[i].bssid[2], survey.sites[i].bssid[3],
			 survey.sites[i].bssid[4], survey.sites[i].bssid[5]);
		snprintf(index_str, 2, "%d", i);

		if (count > 1) {
			json_str_init(jptr, content, HTTPD_MAX_MESSAGE);
			json_start_object(jptr);
		}
		json_start_array(jptr);
		json_set_array_str(jptr, survey.sites[i].ssid);
		json_set_array_str(jptr, tempstr);
		json_set_array_int(jptr, mode);
		json_set_array_int(jptr, survey.sites[i].channel);
		json_set_array_int(jptr, -(survey.sites[i].rssi));
		json_set_array_int(jptr, bcn_nf_last);
		json_close_array(jptr);

		if (count > 1) {
			/* This will overwrite '{' character set by
			 * json_object_init call with ','for array objects
			 * after first network.
			 */
			*content = ',';
		}

		if ((survey.num_sites - i) != 1) {

			err = httpd_send_chunk(req->sock, content,
					strlen(content));
			if (err == -WM_FAIL) {
				/* Release survey.lock mutex */
				os_mutex_put(&survey.lock);
				goto out;
			}
		}
	}

	json_pop_array_object(jptr);
	json_close_object(jptr);
	err = httpd_send_chunk(req->sock, content, strlen(content));
	if (err == -WM_FAIL) {
		/* Release survey.lock mutex */
		os_mutex_put(&survey.lock);
		goto out;
	}

	os_mutex_put(&survey.lock);
	err = httpd_send_chunk(req->sock, NULL, 0);
out:
	os_mem_free(content);
	if (scan_policy == PROV_ON_DEMAND_SCAN) {
		os_semaphore_put(&os_sem_on_demand_scan);
	}
	return err;
}

/* This function is used only in normal mode */
int set_network_cb_register(int (*cb_fn) (struct wlan_network *))
{
	network_set_cb = cb_fn;
	if (httpd_register_wsgi_handlers(&http_handle_network, 1) != 0) {
		prov_e("Failed to register web hanlders");
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

static struct httpd_wsgi_call provisioning_scan_web_handlers[] = {
	{"/sys/scan", HTTPD_DEFAULT_HDR_FLAGS, 0,
		wsgi_handler_get_scan_results, NULL, NULL, NULL},
	{"/sys/scan-config", HTTPD_DEFAULT_HDR_FLAGS, 0,
		get_scan_config_handler, set_scan_config_handler,
		NULL, NULL}
};

int register_provisioning_web_handlers(void)
{

	if (httpd_register_wsgi_handlers(provisioning_scan_web_handlers, 2)
			!= WM_SUCCESS ||
			httpd_register_wsgi_handlers(&http_handle_network, 1)
			!= WM_SUCCESS) {
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

void unregister_provisioning_web_handlers(void)
{
	/* /sys/network is not unregistered since it is required in normal
	 * mode. */
	httpd_unregister_wsgi_handlers(provisioning_scan_web_handlers, 2);
	return;
}
