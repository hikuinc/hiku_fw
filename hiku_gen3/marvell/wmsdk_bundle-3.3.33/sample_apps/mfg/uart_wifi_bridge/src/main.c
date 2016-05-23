/*
 *  Copyright (C)2008 -2014, Marvell International Ltd.
 *  All Rights Reserved.
 */

/**
 *  @brief This is the WiFi Calibration application
 */
#include <wm_os.h>
#include <mdev.h>
#include <mdev_uart.h>
#include <mdev_sdio.h>
#include <lowlevel_drivers.h>
#include <cli.h>
#include <wlan.h>
#include <partition.h>
#include <flash.h>
#include <wmstdio.h>
#include "uart_wifi_bridge.h"
#ifdef APPCONFIG_BT_SUPPORT
#include <bt.h>
#endif

#define HCI_COMMAND_PKT         0x01
#define HCI_ACLDATA_PKT         0x02
#define HCI_SCODATA_PKT         0x03
#define HCI_EVENT_PKT           0x04
#define HCI_VENDOR_PKT          0xff

/* #define DEBUG_LOG */

extern int sd_wifi_init(enum wlan_type type,
			enum wlan_fw_storage_type st,
			flash_desc_t *fl, uint8_t *fw_ram_start_addr);
void bt_unmask_sdio_interrupts();

typedef unsigned short t_u16;
/** Interface header length */
#define INTF_HEADER_LEN     4
#define SDIOPKTTYPE_CMD     0x1

mdev_t *dev;
void read_wlan_resp();

static os_thread_t wifi_bridge_uart_thread;
/* To do: Stack size may be reduced */
static os_thread_stack_define(wifi_bridge_uart_thread_stack, 5 * 2048);

uint8_t *local_outbuf;

static SDIOPkt *sdiopkt;
static uint8_t *rx_buf;
static uart_cb uartcb;

static int send_response_to_uart(uart_cb *uart, uint8_t *resp, int type);

#ifdef APPCONFIG_BT_SUPPORT
os_semaphore_t bt_sem;
struct hci_cmd {
	uint16_t ocf_ogf;
	uint8_t len;
	uint8_t data[256];
} __packed;

static void bt_pkt_recv(uint8_t pkt_type, uint8_t *data, uint32_t size)
{
	uart_cb *uart = &uartcb;
#ifdef DEBUG_LOG
	wmprintf("\r\nBT Packet Received: pkt_type 0x%x\r\n", pkt_type);
	dump_hex(data, size);
#endif
	send_response_to_uart(uart, data, 2);
	memset(uart->uart_buf, 0, sizeof(uart->uart_buf));
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

void bt_packet_send(const uint8_t *packet, uint32_t length)
{
	/* This contains the command to be sent. This does not have the 4 byte
	 * uart header and 12 byte cmd header. Instead, it has 4 byte sdio
	 * header attached */
	struct hci_cmd cmd;

	memset(&cmd, 0, sizeof(struct hci_cmd));

	memcpy(&cmd.ocf_ogf, &packet[1], 2);
	cmd.len = length - 8;
	memcpy(cmd.data, packet + 4, cmd.len);
	bt_drv_send(HCI_COMMAND_PKT, (uint8_t *)&cmd, cmd.len + 3);
}

#endif

static cmd_header last_cmd_hdr;	/* holds the last cmd_hdr */

static void uart_init_crc32(uart_cb *uartcb)
{
	int i, j;
	unsigned int c;
	for (i = 0; i < 256; ++i) {
		for (c = i << 24, j = 8; j > 0; --j)
			c = c & 0x80000000 ? (c << 1) ^ CRC32_POLY : (c << 1);
		uartcb->crc32_table[i] = c;
	}
}

static uint32_t uart_get_crc32(uart_cb *uart, int len, unsigned char *buf)
{
	unsigned int *crc32_table = uart->crc32_table;
	unsigned char *p;
	unsigned int crc;
	crc = 0xffffffff;
	for (p = buf; len > 0; ++p, --len)
		crc = (crc << 8) ^ (crc32_table[(crc >> 24) ^ *p]);
	return ~crc;
}

/*
	send_response_to_uart() handles the response from the firmware.
	This involves
	1. replacing the sdio header with the uart header
	2. computation of the crc of the payload
	3. sending it out to the uart
*/

static int send_response_to_uart(uart_cb *uart, uint8_t *resp, int type)
{
	uint32_t bridge_chksum = 0;
	uint32_t msglen;
	int index;
	uint32_t payloadlen;
	uart_header *uart_hdr;
	SDIOPkt *sdio = (SDIOPkt *) resp;

	int iface_len;

	if (type == 2)
		/* This is because, the last byte of the sdio header
		 * (packet type) is also requried by the labtool, to
		 * understand the type of packet and take appropriate action */
		iface_len = INTF_HEADER_LEN - 1;
	else
		iface_len = INTF_HEADER_LEN;

	payloadlen = sdio->size - iface_len;
	memset(rx_buf, 0, BUF_LEN);
	memcpy(rx_buf + sizeof(uart_header) + sizeof(cmd_header),
	       resp + iface_len, payloadlen);

	/* Added to send correct cmd header len */
	cmd_header *cmd_hdr;
	cmd_hdr = &last_cmd_hdr;
	cmd_hdr->length = payloadlen + sizeof(cmd_header);

	memcpy(rx_buf + sizeof(uart_header), (uint8_t *) &last_cmd_hdr,
	       sizeof(cmd_header));

	uart_hdr = (uart_header *) rx_buf;
	uart_hdr->length = payloadlen + sizeof(cmd_header);
	uart_hdr->pattern = 0x5555;

	/* calculate CRC. The uart_header is excluded */
	msglen = payloadlen + sizeof(cmd_header);
	bridge_chksum = uart_get_crc32(uart, msglen, rx_buf +
				       sizeof(uart_header));
	index = sizeof(uart_header) + msglen;

	rx_buf[index] = bridge_chksum & 0xff;
	rx_buf[index + 1] = (bridge_chksum & 0xff00) >> 8;
	rx_buf[index + 2] = (bridge_chksum & 0xff0000) >> 16;
	rx_buf[index + 3] = (bridge_chksum & 0xff000000) >> 24;

	/* write response to uart */
	uart_drv_write(dev, rx_buf, payloadlen + sizeof(cmd_header)
		       + sizeof(uart_header) + 4);
	memset(rx_buf, 0, BUF_LEN);
	wmprintf("Command response sent\r\n");

	return 0;
}

/*
	check_command_complete() validates the command from the uart.
	It checks for the signature in the header and the crc of the
	payload. This assumes that the uart_buf is circular and data
	can be wrapped.
*/

int check_command_complete(uint8_t *buf)
{
	uart_header *uarthdr;
	uint32_t msglen, endofmsgoffset;
	uart_cb *uart = &uartcb;
	int checksum = 0, bridge_checksum = 0;

	uarthdr = (uart_header *) buf;

	/* out of sync */
	if (uarthdr->pattern != 0x5555) {
		wmprintf("Pattern mismatch\r\n");
		return -WM_FAIL;
	}
	/* check crc */
	msglen = uarthdr->length;

	/* add 4 for checksum */
	endofmsgoffset = sizeof(uart_header) + msglen + 4;

	memset((uint8_t *) local_outbuf, 0, sizeof(local_outbuf));
	if (endofmsgoffset < UART_BUF_SIZE) {
		memcpy((uint8_t *) local_outbuf, buf, endofmsgoffset);
	} else {
		memcpy((uint8_t *) local_outbuf, buf, UART_BUF_SIZE);
		/* To do : check if copying method is correct */
		memcpy((uint8_t *) local_outbuf + UART_BUF_SIZE,
		       buf, endofmsgoffset);
	}

	checksum = *(int *)((uint8_t *) local_outbuf + sizeof(uart_header) +
			    msglen);

	bridge_checksum = uart_get_crc32(uart, msglen,
					 (uint8_t *) local_outbuf +
					 sizeof(uart_header));
	if (checksum == bridge_checksum) {
		return WM_SUCCESS;
	}
	/* Reset local outbuf */
	memset(local_outbuf, 0, sizeof(local_outbuf));
	wmprintf("command checksum error\r\n");
	return -WM_FAIL;
}

/*
	process_input_cmd() sends command to the wlan
	card
*/
int wifi_raw_packet_send(const uint8_t *packet, uint32_t length);
int process_input_cmd(uint8_t *buf, int m_len)
{
	uart_header *uarthdr;
	int i, ret = -WM_FAIL;
	uint8_t *s, *d;

	memset(local_outbuf, 0, BUF_LEN);

	uarthdr = (uart_header *) buf;

	/* sdiopkt = local_outbuf */
	sdiopkt->pkttype = SDIOPKTTYPE_CMD;

	sdiopkt->size = m_len - sizeof(cmd_header) + INTF_HEADER_LEN;
	d = (uint8_t *) local_outbuf + INTF_HEADER_LEN;
	s = (uint8_t *) buf + sizeof(uart_header) + sizeof(cmd_header);

	for (i = 0; i < uarthdr->length - sizeof(cmd_header); i++) {
		if (s < buf + UART_BUF_SIZE)
			*d++ = *s++;
		else {
			s = buf;
			*d++ = *s++;
		}
	}

	d = (uint8_t *) &last_cmd_hdr;
	s = (uint8_t *) buf + sizeof(uart_header);

	for (i = 0; i < sizeof(cmd_header); i++) {
		if (s < buf + UART_BUF_SIZE)
			*d++ = *s++;
		else {
			s = buf;
			*d++ = *s++;
		}
	}
	cmd_header *cmd_hd = (cmd_header *) (buf + sizeof(uarthdr));

	switch (cmd_hd->type) {
#ifdef APPCONFIG_BT_SUPPORT
	case TYPE_HCI:
		bt_packet_send(buf+16, m_len - 16);
		ret = 2;
		break;
#endif
	case TYPE_WLAN:
		wifi_raw_packet_send(local_outbuf, BUF_LEN);
		ret = 1;
		break;
	default:
		/* Unknown command. No need to handle */
		break;
	}
	return ret;
}

/*
	uart_rx_cmd() runs in a loop. It polls the uart ring buffer
	checks it for a complete command and sends the command to the
	wlan card
*/

void uart_rx_cmd(os_thread_arg_t arg)
{
	uart_cb *uart = &uartcb;
	uart_init_crc32(uart);
	int uart_rx_len = 0;
	int len = 0;
	int msg_len = 0;
	while (1) {
		len = 0;
		msg_len = 0;
		uart_rx_len = 0;
		memset(uart->uart_buf, 0, sizeof(uart->uart_buf));
		while (len != LABTOOL_PATTERN_HDR_LEN) {
			uart_rx_len = uart_drv_read(dev, uart->uart_buf
						    + len,
						    LABTOOL_PATTERN_HDR_LEN -
						    len);
			len += uart_rx_len;
		}
		/* Length of the packet is indicated by byte[2] & byte[3] of
		the packet excluding header[4 bytes] + checksum [4 bytes]
		*/
		msg_len = (uart->uart_buf[3] << 8) + uart->uart_buf[2];
		len = 0;
		uart_rx_len = 0;
		while (len != msg_len + LABTOOL_PATTERN_HDR_LEN) {
			/* To do: instead of reading 1 byte,
			read whole chunk of data */
			uart_rx_len = uart_drv_read(dev, uart->uart_buf +
						    LABTOOL_PATTERN_HDR_LEN +
						    len, 1);
			len++;
		}
#ifdef DEBUG_LOG
		dump_hex(uart->uart_buf, msg_len +
			 LABTOOL_PATTERN_HDR_LEN + CHECKSUM_LEN);
#endif

		/* validate the command including checksum */
		if (check_command_complete(uart->uart_buf) == WM_SUCCESS) {
			/* send fw cmd over SDIO after
			   stripping off uart header */
			int ret = process_input_cmd(uart->uart_buf,
					msg_len + 8);
			memset(uart->uart_buf, 0, sizeof(uart->uart_buf));
			if (ret == 1)
				read_wlan_resp();
#ifdef APPCONFIG_BT_SUPPORT
			else {
				bt_unmask_sdio_interrupts();
				os_semaphore_get(&bt_sem, OS_WAIT_FOREVER);
			}
#endif
		} else
			uart_drv_rx_buf_reset(dev);
	}
	os_thread_self_complete(NULL);
}

/*
	read_wlan_resp() handles the responses from the wlan card.
	It waits on wlan card interrupts on account
	of command responses are handled here. The response is
	read and then sent through the uart to the Mfg application
*/
int wifi_raw_packet_recv(uint8_t **data, uint32_t *pkt_type);
void read_wlan_resp()
{
	uart_cb *uart = &uartcb;
	uint8_t *packet;
	uint32_t pkt_type;
	int rv = wifi_raw_packet_recv(&packet, &pkt_type);
	if (rv != WM_SUCCESS)
		wmprintf("Receive response failed\r\n");
	else {
		if (pkt_type == MLAN_TYPE_CMD)
			send_response_to_uart(uart, packet, 1);
	}
}

/* Initialize the tasks for uart-wifi-bridge app */
int task_init()
{
	int ret = 0;
#ifdef APPCONFIG_BT_SUPPORT
	bt_drv_init();
	bt_send_reset();
	bt_send_noscan();
	os_thread_sleep(os_msec_to_ticks(200));

	ret = os_semaphore_create(&bt_sem, "bt_sem");
	if (ret)
		wmprintf("Unable to create bt sem\r\n");
	os_semaphore_get(&bt_sem, OS_WAIT_FOREVER);

	bt_drv_set_cb(&bt_pkt_recv);
#endif

	ret = os_thread_create(&wifi_bridge_uart_thread,
			       "wifi_bridge_uart_thread",
			       (void *)uart_rx_cmd, 0,
			       &wifi_bridge_uart_thread_stack, OS_PRIO_3);
	if (ret)
		wmprintf("Failed to create wifi_bridge_uart_thread\n\r");

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
	sdiopkt = (SDIOPkt *) local_outbuf;

	/* Initialize UART */
	uart_drv_init(UART0_ID, UART_8BIT);
	uart_drv_blocking_read(UART0_ID, true);
	dev = uart_drv_open(UART0_ID, BAUD_RATE);

	part_init();
	struct partition_entry *p;
	p = part_get_layout_by_id(FC_COMP_WLAN_FW, NULL);
	if (p == NULL) {
		wmprintf("part get layout failed\r\n");
		return -WM_FAIL;
	}
	flash_desc_t fl;
	part_to_flash_desc(p, &fl);
	rx_buf = os_mem_alloc(BUF_LEN);
	if (rx_buf == NULL)
		wmprintf("failed to allocate memory\r\n");
	int ret = sd_wifi_init(WLAN_TYPE_WIFI_CALIB,
				WLAN_FW_IN_FLASH, &fl, NULL);
	task_init();
	return ret;
}
