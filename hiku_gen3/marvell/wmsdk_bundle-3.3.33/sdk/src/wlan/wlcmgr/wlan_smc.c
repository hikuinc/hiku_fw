/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wlan.h>
#include <wlan_smc.h>
#include <wifi.h>

extern void sniffer_register_callback(void (*sniffer_cb)
				(const wlan_frame_t *frame,
				 const uint16_t len));
extern void wlan_set_smart_mode_active();
extern void wlan_set_smart_mode_inactive();
extern bool wlan_get_smart_mode_status();

int wlan_print_smart_mode_cfg()
{
	return wifi_get_smart_mode_cfg();
}

#define FIRST_2_4_CHANNEL 1
#define NUM_2_4_CHANNELS 14
static wifi_chan_list_param_set_t *populate_channel_information(
	struct wlan_network *network_info, smart_mode_cfg_t *cfg)
{
	int chan, first_chan, num_chan, cnt_chan = 0;

	if (cfg->channel) {
		first_chan = cfg->channel - 1;
		num_chan = 3;
	} else {
		first_chan = FIRST_2_4_CHANNEL;
		num_chan = NUM_2_4_CHANNELS;
	}

	for (chan = first_chan; chan < (first_chan + num_chan); chan++) {
		if (wifi_11d_is_channel_allowed(chan))
			cnt_chan++;
	}

	wifi_chan_list_param_set_t *chan_list = os_mem_alloc(
		sizeof(wifi_chan_list_param_set_t) +
		(sizeof(wifi_chan_scan_param_set_t) * (cnt_chan)));
	chan_list->no_of_channels = cnt_chan + 1;

	/* Populate uAP channel information at index 0 of chan_list */
	chan_list->chan_scan_param[0].chan_number = network_info->channel;
	chan_list->chan_scan_param[0].min_scan_time = 30;
	chan_list->chan_scan_param[0].max_scan_time = 30;

	cnt_chan = 1;
	for (chan = first_chan; chan < (first_chan + num_chan); chan++) {
		if (!wifi_11d_is_channel_allowed(chan))
			continue;
		chan_list->chan_scan_param[cnt_chan].chan_number = chan;
		chan_list->chan_scan_param[cnt_chan].min_scan_time =
			cfg->min_scan_time;
		chan_list->chan_scan_param[cnt_chan].max_scan_time =
			cfg->max_scan_time;
		cnt_chan++;
	}
	return chan_list;
}

#define COUNTRY_INFO_TYPE 0x07
static void populate_country_code_information(smart_mode_cfg_t *cfg)
{
	int nr_sb;
	uint8_t *ptr = cfg->custom_ie + cfg->custom_ie_len;
	wifi_sub_band_set_t *sub_band = get_sub_band_from_country(
		cfg->country_code, &nr_sb);
	uint8_t country_info_type = COUNTRY_INFO_TYPE;
	uint8_t sub_band_len = sizeof(wifi_sub_band_set_t) * nr_sb;
	uint8_t country_info_len = sub_band_len + COUNTRY_CODE_LEN;

	*ptr = country_info_type;
	cfg->custom_ie_len += sizeof(country_info_type);
	ptr += sizeof(country_info_type);

	*ptr = country_info_len;
	cfg->custom_ie_len += sizeof(country_info_len);
	ptr += sizeof(country_info_len);

	memcpy(ptr, wlan_11d_country_index_2_string(cfg->country_code),
	       COUNTRY_CODE_LEN);
	cfg->custom_ie_len += COUNTRY_CODE_LEN;
	ptr += COUNTRY_CODE_LEN;

	memcpy(ptr, sub_band, sub_band_len);
	cfg->custom_ie_len += sub_band_len;
	ptr += sub_band_len;
}

int wlan_set_smart_mode_cfg(struct wlan_network *uap_network,
			    smart_mode_cfg_t *cfg, void (*sniffer_callback)
			    (const wlan_frame_t *frame, const uint16_t len))
{
	int ret;
	uint8_t *smc_start_addr = NULL, *smc_end_addr = NULL;
	uint8_t mac_zero[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	wifi_chan_list_param_set_t *channel_list;
	if (!cfg->country_code)
		wlan_set_country(COUNTRY_US);
	channel_list = populate_channel_information(uap_network, cfg);
	populate_country_code_information(cfg);
	sniffer_register_callback(sniffer_callback);
	if (memcmp(cfg->smc_start_addr, mac_zero, sizeof(MLAN_MAC_ADDR_LENGTH))
	   &&
	   memcmp(cfg->smc_end_addr, mac_zero, sizeof(MLAN_MAC_ADDR_LENGTH))) {
		smc_start_addr = cfg->smc_start_addr;
		smc_end_addr = cfg->smc_end_addr;
	}
	ret = wifi_set_smart_mode_cfg(uap_network->ssid, cfg->beacon_period,
				       channel_list, smc_start_addr,
				       smc_end_addr, cfg->filter_type,
				       cfg->smc_frame_filter_len,
				       cfg->smc_frame_filter,
				       cfg->custom_ie_len, cfg->custom_ie);
	os_mem_free(channel_list);
	return ret;
}

int wlan_start_smart_mode()
{
	if (wlan_get_smart_mode_status() == true)
		return WM_SUCCESS;
	wlan_set_smart_mode_active();
	return wifi_start_smart_mode();
}

int wlan_stop_smart_mode()
{
	if (wlan_get_smart_mode_status() == false)
		return WM_SUCCESS;
	wlan_set_smart_mode_inactive();
	return wifi_stop_smart_mode();
}

int wlan_stop_smart_mode_start_uap(struct wlan_network *uap_network)
{
	int ret;
	uint8_t mac_addr[6];

	ret = wlan_stop_smart_mode();
	if (ret != WM_SUCCESS) {
		wlcm_e("Failed to stop smart mode");
		return ret;
	}

	uap_network->type = WLAN_BSS_TYPE_UAP;
	wlan_get_mac_address(mac_addr);
	uap_network->security.type = WLAN_SECURITY_NONE;

	wlan_remove_network(uap_network->name);

	ret = wlan_add_network(uap_network);
	if (ret != WM_SUCCESS) {
		return -WM_FAIL;
	}
	ret = wlan_start_network(uap_network->name);
	if (ret != WM_SUCCESS) {
		return -WM_FAIL;
	}
	if (ret) {
		wlcm_e("uAP start failed, giving up");
		return -WM_FAIL;
	}
	return WM_SUCCESS;
}
