/*
 *  Copyright (C)2008 -2014, Marvell International Ltd.
 *  All Rights Reserved.
 */

/**
 *  @brief This is the HCI Bridge Application
 */
#include <wm_os.h>
#include <mdev.h>
#include <mdev_uart.h>
#include <mdev_sdio.h>
#include <cli.h>
#include <wlan.h>
#include <partition.h>
#include <flash.h>
#include <wmstdio.h>
#include <bt.h>
#include "hci_bridge.h"

/* #define DEBUG_LOG */

#define INTF_HEADER_LEN 4

#define HCI_COMMAND_PKT         0x01
#define HCI_ACLDATA_PKT         0x02
#define HCI_SCODATA_PKT         0x03
#define HCI_EVENT_PKT           0x04
#define HCI_VENDOR_PKT          0xff

os_semaphore_t bt_sem;
mdev_t *dev;

uint8_t *local_outbuf;

unsigned char rd_buf[SDIO_OUTBUF_LEN];

struct hci_cmd {
	uint16_t ocf_ogf;
	uint8_t len;
	uint8_t data[256];
} __packed;

static void bt_pkt_recv(uint8_t pkt_type, uint8_t *data, uint32_t size)
{
#ifdef DEBUG_LOG
	wmprintf("\r\nBT Packet Received: pkt_type 0x%x\r\n", pkt_type);
	dump_hex(data, size);
#endif
	memcpy(local_outbuf, data, size);
	os_semaphore_put(&bt_sem);
}

static void bt_send_reset()
{
	struct hci_cmd cmd;

	memset(&cmd, 0, sizeof(struct hci_cmd));
	cmd.ocf_ogf = (0x3 << 10) | 0x3;
	cmd.len = 0;

	bt_drv_send(HCI_COMMAND_PKT, (uint8_t *)&cmd, cmd.len + 3);
}


static void bt_send_noscan()
{
	struct hci_cmd cmd;

	memset(&cmd, 0, sizeof(struct hci_cmd));
	/* OCF | (OGF << 10) */
	cmd.ocf_ogf = (0x3 << 10) | 0x1a;
	cmd.len = 1;
	cmd.data[0] = 0x0;

	bt_drv_send(HCI_COMMAND_PKT, (uint8_t *)&cmd, cmd.len + 3);
}

int uart_recv_msg(mdev_t *dev, unsigned char *buf, int buflen)
{
	int remaining_bytes = 0, total_bytes = 0, headers_done = 0;
	int i = 0, cnt, header_length = 0;
	while (1) {
		if (headers_done == 0) {
			cnt = uart_drv_read(dev, &rd_buf[i], 1);
			if (cnt != 1)
				return -WM_FAIL;
			i++;
			switch (rd_buf[0]) {
			case 1:
			case 3:
				header_length = 3;
				break;
			case 2:
				header_length = 4;
				break;
			default:
				wmprintf("Incorrect type of"
					" packet\r\n");
				return -WM_FAIL;
			}

			if (i == header_length + 1) {
				headers_done = 1;
				if (header_length == 3) {
					remaining_bytes = (int)(rd_buf[3]);
				} else {
					remaining_bytes = (int)(rd_buf[3] |
						(rd_buf[4] << 8));
				}
				total_bytes = 1 + header_length +
					remaining_bytes;

				if (total_bytes > buflen) {
					wmprintf("Input len greater than buffer"
						" size\r\n");
					return -WM_FAIL;
				}
				if (remaining_bytes == 0) {
					memcpy(buf, rd_buf, i);
					return i;
				}
			}
		} else {
			cnt = uart_drv_read(dev, &rd_buf[i],
					remaining_bytes);
			if (cnt <= 0)
				return -WM_FAIL;
			remaining_bytes -= cnt;
			i += cnt;

			if (remaining_bytes == 0) {
				memcpy(buf, rd_buf, i);
				return i;
			}
		}
	}
}

static int uart_send_response(mdev_t *dev, uint8_t *resp)
{
	uint32_t payloadlen;
	SDIOPkt *sdio = (SDIOPkt *) resp;
	int iface_len;

	iface_len = INTF_HEADER_LEN - 1;

	payloadlen = sdio->size - iface_len;
	/* write response to uart */
#ifdef DEBUG_LOG
	wmprintf("Writing to uart:\r\n");
	dump_hex(&resp[iface_len], payloadlen);
#endif
	uart_drv_write(dev, &resp[iface_len], payloadlen);
	return 0;
}

void bt_packet_send(uint8_t pkt_type, const uint8_t *packet, uint32_t length)
{
#ifdef DEBUG_LOG
	wmprintf("pkt type is: %d\r\n", pkt_type);
#endif
	bt_drv_send(pkt_type, (uint8_t *)packet, length);
}

static int handle_bt()
{
	int ret = 0;

	/* Create BT sem for synchronization */
	ret = os_semaphore_create(&bt_sem, "bt_sem");
	if (ret) {
		wmprintf("Unable to create BT sem\r\n");
		return -WM_FAIL;
	}
	os_semaphore_get(&bt_sem, OS_WAIT_FOREVER);

	/* Initialize BT */
	bt_drv_init();

	wmprintf("Sending reset cmd\r\n");
	bt_send_reset();
	wmprintf("Sending noscan cmd\r\n");
	bt_send_noscan();
	wmprintf("BT initialization done\r\n");

	os_thread_sleep(os_msec_to_ticks(100));
	bt_drv_set_cb(&bt_pkt_recv);

	while (1) {
		int ret = uart_recv_msg(dev, local_outbuf, SDIO_OUTBUF_LEN);
		if (ret < 0) {
			wmprintf("Error\r\n");
			return ret;
		}
#ifdef DEBUG_LOG
		wmprintf("Read %d bytes\r\n", ret);
		dump_hex(local_outbuf, ret);
#endif
		bt_packet_send(local_outbuf[0], &local_outbuf[1], ret - 1);
		/* Blocked till resp is received */
		os_semaphore_get(&bt_sem, OS_WAIT_FOREVER);
		uart_send_response(dev, local_outbuf);
	}

	return 0;
}

/**
 * All application specific initialization is performed here
 */

int main()
{
	wmstdio_init(UART1_ID, 0);
	local_outbuf = os_mem_alloc(SDIO_OUTBUF_LEN);
	if (local_outbuf == NULL) {
		wmprintf("Failed to allocate buffer\r\n");
		return -WM_FAIL;
	}
	/* Initialize UART */
	uart_drv_init(UART0_ID, UART_8BIT);
	uart_drv_blocking_read(UART0_ID, true);
	dev = uart_drv_open(UART0_ID, BAUD_RATE);

	/* Initialize WLAN */
	part_init();
	struct partition_entry *p;
	p = part_get_layout_by_id(FC_COMP_WLAN_FW, NULL);
	if (p == NULL) {
		wmprintf("part get layout failed\r\n");
		return -WM_FAIL;
	}
	flash_desc_t fl;
	part_to_flash_desc(p, &fl);
	int ret = wlan_init(&fl);
	if (ret != WM_SUCCESS) {
		wmprintf("wlan_init failed\r\n");
		return -WM_FAIL;
	}

	ret = handle_bt();
	if (ret != WM_SUCCESS)
		wmprintf("handle_bt failed\r\n");

	return ret;
}
