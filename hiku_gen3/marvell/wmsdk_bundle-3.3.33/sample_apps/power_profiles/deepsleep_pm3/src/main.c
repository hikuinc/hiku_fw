/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 * Power profile : Wi-Fi Deepsleep + PM3
 *
 * Summary:
 * This application is a demo which illustrates
 * Wi-Fi  Deepsleep and MCU PM3 power profile
 *
 * Description:
 * This application contains following actions
 * -# Device boot-up
 * -# Wi-Fi firmware download
 * -# Device connects to a hardcoded network
 * -# Transmits a UDP packet
 * -# Disconnects from the network
 * -# Wi-Fi is put in  deepsleep
 * -# MCU enters PM3 using power management framework
 * -# Wakeup from PM3
 * -# Exit from PM3
 * -# Exit from Deepsleep
 * -# Re-connect to network and repeat
 * This application cn be used as a start point
 * for battery operated sensors which post data
 * periodically with a higher periodicity.
 *
 * This application does not use application
 * framework.
 *
 *
 *  Things to change:
 *  NETWORK_SSID: Network name to which device will connect
 *  NETWORK_PASSPHRASE: Passphrase of network
 *  NETWORK_SECURITY:  Network Security
 *			0: open
 *			3: WPA
 *			4: WPA2
 *			5: Mix mode
 **/

#include <wmstdio.h>
#include <wm_os.h>
#include <lowlevel_drivers.h>
#include <pwrmgr.h>
#include <wmtime.h>
#include <cli.h>
#include <stdlib.h>
#include <board.h>
#include <wlan.h>
#include <partition.h>
#include <mdev_i2c.h>
#include <psm-v2.h>
#include <psm.h>
#include <wm_net.h>

#define DEF_NET_NAME            "default"
#define NETWORK_SSID           "ssid" /* Modify this with actual SSID */
#define NETWORK_PASSPHRASE     "pasphrase" /* Modify this with actual
					      passphrase */
#define NETWORK_SECURITY       4 /* Modify this with actual security as per
				    values below */

/* Security Type:
 * 0 : No security
 * 3 : WPA
 * 4 : WPA2
 * 5 : WPA/WPA2 mixed mode */

#define SLEEP_MSEC             60000
#define THRESHOLD_TIME         10
#define UDP_BROADCAST_PORT     55555
#define SENSOR_WAIT_TIME       10

typedef enum {
	EV_ENTER_STATE = 0,
	EV_WLAN_INITIALIZED,
	EV_WLAN_SUCCESS,
	EV_SENSOR_READY,
	EV_WLAN_USER_DISCONNECT,
	EV_WLAN_PS_ENTER,
	EV_WLAN_PS_EXIT,

} dev_event_t;

typedef enum {
	STATE_DEFAULT_BOOT = 0,
	STATE_WLAN_INIT,
	STATE_WLAN_NOT_CONNECTED_SENSOR_NOT_READY,
	STATE_WLAN_CONNECTED_SENSOR_NOT_READY,
	STATE_WLAN_NOT_CONNECTED_SENSOR_READY,
	STATE_READY_XMIT,
	STATE_WLAN_DISCONNECT_REQ,
	STATE_PS_ENTER_REQ,
	STATE_PS_EXIT_REQ,

} dev_state_t;

static os_queue_pool_define(wlan_event_queue_data, sizeof(dev_event_t) * 4);
static os_queue_t wlan_event_queue;

static os_semaphore_t sensor_sem;
static os_thread_stack_define(sensor_stack, 512);
static os_thread_t sensor_thread;

static struct wlan_network wlan_config;
static char broadcast_buf[10];
static struct sockaddr_in broadcast_addr;
static int bsock;


int get_nw(struct wlan_network *network)
{
	memset(network, 0, sizeof(*network));
	memcpy(network->name, DEF_NET_NAME, strlen(DEF_NET_NAME) + 1);

	sprintf(network->ssid, NETWORK_SSID);

	network->security.type = NETWORK_SECURITY;
	network->ssid_specific = 1;
	network->ip.ipv4.addr_type = ADDR_TYPE_DHCP;

	/* These are not required for open security */
	sprintf(network->security.psk, NETWORK_PASSPHRASE);
	network->security.psk_len = strlen(network->security.psk);

	return WM_SUCCESS;
}

/*
 * This is a callback handler registered with wlan subsystem. This callback
 * gets invoked on occurrence of any wlan event
 */
static int wlan_event_callback(enum wlan_event_reason event, void *data)
{
	dev_event_t ev;
	switch (event) {
	case WLAN_REASON_INITIALIZED:
		ev = EV_WLAN_INITIALIZED;
		os_queue_send(&wlan_event_queue, &ev, OS_NO_WAIT);
		break;
	case WLAN_REASON_SUCCESS:
		ev = EV_WLAN_SUCCESS;
		os_queue_send(&wlan_event_queue, &ev, OS_NO_WAIT);
		break;
	case WLAN_REASON_USER_DISCONNECT:
		ev = EV_WLAN_USER_DISCONNECT;
		os_queue_send(&wlan_event_queue, &ev, OS_NO_WAIT);
		break;
	case WLAN_REASON_PS_ENTER:
		ev = EV_WLAN_PS_ENTER;
		os_queue_send(&wlan_event_queue, &ev, OS_NO_WAIT);
		break;
	case WLAN_REASON_PS_EXIT:
		ev = EV_WLAN_PS_EXIT;
		os_queue_send(&wlan_event_queue, &ev, OS_NO_WAIT);
		break;
	default:
		wmprintf("Other event: %d\r\n", event);

	}
	return WM_SUCCESS;
}

int init_network()
{
	int ret;
	struct partition_entry *p;
	flash_desc_t fl;
	/* Read partition table layout from flash */
	part_init();

	p = part_get_layout_by_id(FC_COMP_PSM, NULL);
	part_to_flash_desc(p, &fl);
	psm_hnd_t psm_hnd;
	psm_module_init(&fl, &psm_hnd, NULL);
	psm_cli_init();

	p = part_get_layout_by_id(FC_COMP_WLAN_FW, NULL);
	if (p == NULL) {
		wmprintf("Fw not found\r\n");
		return -WLAN_ERROR_FW_NOT_DETECTED;
	}
	part_to_flash_desc(p, &fl);

	/* Initialize wlan */
	ret = wlan_init(&fl);
	if (ret) {
		wmprintf("wlan_init failed %d\r\n", ret);
		return ret;
	}
	ret = wlan_start(wlan_event_callback);
	return ret;
}

int test_init()
{
	int ret = 0;
	wmstdio_init(UART0_ID, 0);
	pm_init();
	wmtime_init();
	if (init_network()) {
		wmprintf("init_network error\r\n");
		return -WM_FAIL;
	}
	return ret;
}

void udp_broadcast_init(struct sockaddr_in *broadcast_addr)
{
	memset(broadcast_addr, 0, sizeof(*broadcast_addr));
	broadcast_addr->sin_family = AF_INET;
	broadcast_addr->sin_port = htons(UDP_BROADCAST_PORT);
}


static int initialize_socket()
{
	/* Setting all bytes to 0xde. Please change it to senosr data */
	memset(broadcast_buf, 0xde, sizeof(broadcast_buf));
	udp_broadcast_init(&broadcast_addr);
	bsock = socket(AF_INET, SOCK_DGRAM, 0);
	if (bsock < 0) {
		wmprintf("Error in creating socket\r\n");
		return -WM_FAIL;
	}
	int on = 1;
	if (setsockopt(bsock, SOL_SOCKET, SO_BROADCAST,
		       (char *)&on, sizeof(on)) < 0) {
		wmprintf("Failed to set socket option");
		return -WM_FAIL;
	}
	return WM_SUCCESS;
}

static int broadcast_udp_packets()
{

	int i = 0, status, ret = WM_SUCCESS;
	struct wlan_ip_config addr;
	wlan_get_address(&addr);
	broadcast_addr.sin_addr.s_addr = addr.ipv4.address |
		~(addr.ipv4.netmask);
	while (i < 3) {
		os_thread_sleep(os_msec_to_ticks(5));
		status = sendto(bsock, broadcast_buf,
				sizeof(broadcast_buf), 0,
				(struct sockaddr *)&broadcast_addr,
				sizeof(broadcast_addr));
		if (status <= 0)
			ret++;
		i++;
	}
	return ret;
}

static void read_temperature_main(os_thread_arg_t data)
{
	dev_event_t ev = EV_SENSOR_READY;
	/* Perform sensor specific initialization */
	while (1) {
		os_semaphore_get(&sensor_sem, OS_WAIT_FOREVER);
		/* Perform sensor operation: read temperature */
		os_thread_sleep(os_msec_to_ticks(SENSOR_WAIT_TIME));
		os_queue_send(&wlan_event_queue, &ev, OS_NO_WAIT);
	}
}


/**
 * All application specific initialization is performed here
 */
int main(void)
{
	int ret;
	dev_event_t ev;
	dev_state_t state;

	ret = test_init();
	if (ret) {
		return -WM_FAIL;
	}

	wmprintf("pm3_ds: Sample application Started\r\n");

	ret = os_queue_create(&wlan_event_queue, "wlan_event_queue",
			      sizeof(dev_event_t), &wlan_event_queue_data);
	if (ret != WM_SUCCESS) {
		wmprintf("Failed to create wlan event queue\r\n");
		return -WM_FAIL;
	}

	ret = os_semaphore_create(&sensor_sem, "sensor_sem");
	if (ret) {
		wmprintf("Error creating sensor semaphore\r\n");
		return -WM_FAIL;
	}
	os_semaphore_get(&sensor_sem, OS_WAIT_FOREVER);

	ret = os_thread_create
		(&sensor_thread,
		"sensor_temp",
		read_temperature_main,
		0,
		&sensor_stack,
		OS_PRIO_3);

	if (ret != WM_SUCCESS) {
		wmprintf("Thread creation error");
		return -WM_FAIL;
	}

	state = STATE_DEFAULT_BOOT;

	while (1) {
		ret = os_queue_recv(&wlan_event_queue, &ev, OS_WAIT_FOREVER);
		if (ret != WM_SUCCESS) {
			wmprintf("Failed to receive event on the queue\r\n");
			continue;
		}
begin:
		switch (state) {
		case STATE_DEFAULT_BOOT:
			if (ev != EV_WLAN_INITIALIZED) {
				wmprintf("Critical error\r\n");
				return -WM_FAIL;
			}
			get_nw(&wlan_config);
			ret = wlan_add_network(&wlan_config);
			if (ret != WM_SUCCESS) {
				wmprintf("Failed to add network %d\r\n",
					 ret);
				return -WM_FAIL;
			}
			ret = initialize_socket();
			if (ret != WM_SUCCESS) {
				wmprintf("Failed to initialize socket"
					 "\r\n");
				return -WM_FAIL;
			}
			state = STATE_WLAN_INIT;
			/* fall through */
		case STATE_WLAN_INIT:
			ret = wlan_connect(wlan_config.name);
			if (ret) {
				wmprintf("Error sending connect %d\r\n"
					 , ret);
				return -WM_FAIL;
			}
			os_semaphore_put(&sensor_sem);
			state =
				STATE_WLAN_NOT_CONNECTED_SENSOR_NOT_READY;
			break;
		case STATE_WLAN_NOT_CONNECTED_SENSOR_NOT_READY:
			if (ev == EV_WLAN_SUCCESS)
				state = STATE_WLAN_CONNECTED_SENSOR_NOT_READY;
			else if (ev == EV_SENSOR_READY)
				state = STATE_WLAN_NOT_CONNECTED_SENSOR_READY;
			break;
		case STATE_WLAN_CONNECTED_SENSOR_NOT_READY:
			if (ev == EV_SENSOR_READY)
				state = STATE_READY_XMIT;
			break;
		case STATE_WLAN_NOT_CONNECTED_SENSOR_READY:
			if (ev == EV_WLAN_SUCCESS)
				state = STATE_READY_XMIT;
			break;
		case STATE_READY_XMIT:
			ret = broadcast_udp_packets();
			if (ret != WM_SUCCESS)
				wmprintf("Failed to send %d udp "
					 "broadcast packets\r\n", ret);
			ret = wlan_disconnect();
			if (ret != WM_SUCCESS) {
				wmprintf("Failed to send disconnect\r\n");
				return -WM_FAIL;
			}
			state = STATE_WLAN_DISCONNECT_REQ;
			break;
		case STATE_WLAN_DISCONNECT_REQ:
			if (ev == EV_WLAN_USER_DISCONNECT) {
				wlan_deepsleepps_on();
				state = STATE_PS_ENTER_REQ;
			}
			break;
		case STATE_PS_ENTER_REQ:
			if (ev == EV_WLAN_PS_ENTER) {
				pm_mcu_cfg(true, PM3, THRESHOLD_TIME);

				os_thread_sleep(os_msec_to_ticks(SLEEP_MSEC));
				pm_mcu_cfg(false, PM3, 0);
				wlan_deepsleepps_off();
				state = STATE_PS_EXIT_REQ;
			}
			break;
		case STATE_PS_EXIT_REQ:
			if (ev == EV_WLAN_INITIALIZED)
				state = STATE_WLAN_INIT;

			break;
		}
		if (state == STATE_READY_XMIT || state == STATE_WLAN_INIT) {
			goto begin;
		}

	}
}

