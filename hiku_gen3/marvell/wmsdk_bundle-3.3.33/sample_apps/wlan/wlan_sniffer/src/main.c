/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Simple sniffer application
 *
 * Summary:
 *
 * This application configures the Wi-Fi firmware to act as a
 * sniffer. Important information about all the frames received is printed on
 * the console.
 *
 * Description:
 *
 * The application is written using Application Framework that
 * simplifies development of WLAN networking applications.
 *
 * WLAN Initialization:
 *
 * When the application framework is started, it starts up the WLAN
 * sub-system and initializes the network stack. The app receives the event
 * when the WLAN subsystem has been started and initialized.
 *
 * The application starts the sniffer mode of the Wi-Fi module. A sniffer
 * callback is registered that gets called whenever a frame is received from the
 * firmware.
 *
 * The sniffer callback prints important information about the beacon or data
 * packets that are received.
 *
 */

#include <wm_os.h>
#include <app_framework.h>
#include <wmtime.h>
#include <partition.h>
#include <appln_cb.h>
#include <appln_dbg.h>
#include <cli.h>
#include <cli_utils.h>
#include <wlan.h>
#include <wlan_smc.h>
#include <psm.h>
#include <wmstdio.h>
#include <wmsysinfo.h>
#include <wm_net.h>
#include <httpd.h>
#include <wlan_11d.h>
#include <critical_error.h>

/*-----------------------Global declarations----------------------*/

static os_semaphore_t app_sniffer;
struct wlan_network uap_network;

#define MYSSID "WLAN Smart Config AP"

/* This function is defined for handling critical error.
 * For this application, we just stall and do nothing when
 * a critical error occurs.
 */
void critical_error(int crit_errno, void *data)
{
	dbg("Critical Error %d: %s\r\n", crit_errno,
			critical_error_msg(crit_errno));
	while (1)
		;
	/* do nothing -- stall */
}

void process_frame(const wlan_frame_t *frame)
{
	uint8_t client[MLAN_MAC_ADDR_LENGTH];
	wlan_get_mac_address(client);

	if (frame->frame_type == PROBE_REQ &&
	    frame->frame_data.probe_req_info.ssid_len) {

		/* To stop smc mode and start uAP on receiving directed probe
		 * request for uAP */
		if (strncmp(frame->frame_data.probe_req_info.ssid, MYSSID,
			    frame->frame_data.probe_req_info.ssid_len))
			return;
		os_semaphore_put(&app_sniffer);

	} else if (frame->frame_type == AUTH &&
		   (memcmp(frame->frame_data.auth_info.dest, client, 6) == 0)) {
		/* To stop smc mode and start uAP on receiving auth request */
		os_semaphore_put(&app_sniffer);

	} else if (frame->frame_type == DATA || frame->frame_type == QOS_DATA) {
		/* Perform necessary operations here */
#if 0
		wmprintf("********* Data Packet Info *********");
		if (frame->frame_data.data_info.frame_ctrl_flags & 0x08)
			wmprintf("\r\nThis is a retransmission\r\n");
		wmprintf("\r\nType: 0x%x", frame->frame_type);
		wmprintf("\r\nFrame Control flags: 0x%x",
				frame->frame_data.data_info.frame_ctrl_flags);
		wmprintf("\r\nBSSID: ");
		print_mac(frame->frame_data.data_info.bssid);
		wmprintf("\r\nSource: ");
		print_mac(frame->frame_data.data_info.src);
		wmprintf("\r\nDestination: ");
		print_mac(frame->frame_data.data_info.dest);
		wmprintf("\r\nSequence Number: %d",
				wlan_get_seq_num(frame->frame_data.
					data_info.seq_frag_num));
		wmprintf("\r\nFragmentation Number: %d",
				wlan_get_frag_num(frame->frame_data.
					data_info.seq_frag_num));
		wmprintf("\r\nQoS Control Flags: 0x%x",
				frame->frame_data.data_info.qos_ctrl);
		wmprintf("\r\n*******************************\r\n");
#endif
	}
}

/** This Sniffer callback is called from a thread with small stack size,
 * So do minimal memory allocations for correct behaviour.
 */
void sniffer_cb(const wlan_frame_t *frame, const uint16_t len)
{
	if (frame) {
		process_frame(frame);
	}
}

/*
 * Handler invoked when WLAN subsystem is ready.
 *
 * The app-framework tells the handler whether there is
 * valid network information stored in persistent memory.
 *
 * The handler can then chose to connect to the network.
 *
 * We ignore the data and just start a Micro-AP network
 * with DHCP service. This will allow a client device
 * to connect to us and receive a valid IP address via
 * DHCP.
 */
void event_wlan_init_done(void *data)
{
	int ret;
	/*
	 * Initialize CLI Commands for some of the modules:
	 *
	 * -- wlan: allows user to explore basic wlan functions
	 */

	ret = wlan_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_cli_init failed");

	ret = wlan_iw_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_iw_init failed");

	/* List of frame filters for filtering probe requests, authentication
	 * frames and multicast frames */
	uint8_t smc_frame_filter[] = {0x02, 0x10, 0x01, 0x2c, 0x04, 0x02};

	smart_mode_cfg_t smart_mode_cfg;
	bzero(&uap_network, sizeof(struct wlan_network));
	bzero(&smart_mode_cfg, sizeof(smart_mode_cfg_t));
	memcpy(uap_network.ssid, MYSSID, sizeof(MYSSID));
	uap_network.channel = 6;
	smart_mode_cfg.beacon_period = 20;
	smart_mode_cfg.country_code = COUNTRY_US;
	smart_mode_cfg.min_scan_time = 60;
	smart_mode_cfg.max_scan_time = 60;
	smart_mode_cfg.filter_type = 0x03;
	smart_mode_cfg.smc_frame_filter = smc_frame_filter;
	smart_mode_cfg.smc_frame_filter_len = sizeof(smc_frame_filter);
	smart_mode_cfg.custom_ie_len = 0;

	ret = wlan_set_smart_mode_cfg(&uap_network, &smart_mode_cfg,
				      sniffer_cb);

	if (ret != WM_SUCCESS)
		dbg("Error: wlan_set_smart_mode_cfg failed");
	wlan_print_smart_mode_cfg();

	wlan_start_smart_mode();
}

/* This is the main event handler for this project. The application framework
 * calls this function in response to the various events in the system.
 */
int common_event_handler(int event, void *data)
{
	switch (event) {
	case AF_EVT_WLAN_INIT_DONE:
		event_wlan_init_done(data);
		break;
	default:
		break;
	}

	return 0;
}

static void modules_init()
{
	int ret;

	/*
	 * Initialize wmstdio prints
	 */
	ret = wmstdio_init(UART0_ID, 0);
	if (ret != WM_SUCCESS) {
		dbg("Error: wmstdio_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	/*
	 * Initialize CLI Commands
	 */
	ret = cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: cli_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	/* Initialize time subsystem.
	 *
	 * Initializes time to 1/1/1970 epoch 0.
	 */
	ret = wmtime_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: wmtime_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	/*
	 * Register Power Management CLI Commands
	 */
	ret = pm_cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: pm_cli_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	/*
	 * Register Time CLI Commands
	 */
	ret = wmtime_cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: wmtime_cli_init failed");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	return;
}

int main()
{
	modules_init();

	dbg("Build Time: " __DATE__ " " __TIME__ "");

	if (os_semaphore_create(&app_sniffer, "app_sniffer") != WM_SUCCESS) {
		dbg("Failed to create semaphore");
	}
	os_semaphore_get(&app_sniffer, OS_WAIT_FOREVER);

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		dbg("Failed to start application framework");
				critical_error(-CRIT_ERR_APP, NULL);
	}

	os_semaphore_get(&app_sniffer, OS_WAIT_FOREVER);
	wlan_stop_smart_mode();
	app_uap_start_on_channel_with_dhcp(uap_network.ssid, NULL,
					   uap_network.channel);
	os_semaphore_delete(&app_sniffer);
	return 0;
}
