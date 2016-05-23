/** @file uart_wifi_bridge.h
 *
 *  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved
 */

#ifndef _UART_WIFI_BRIDGE_H
#define _UART_WIFI_BRIDGE_H

#define BAUD_RATE 1500000

/* Configure BUF_LEN : length of the buffer used to send wifi packets */
#define BUF_LEN 1024

#define CRC32_POLY 0x04c11db7
/** Type definition:Protocol */

/** UART start pattern*/
typedef struct _uart_header {
    /** pattern */
	short pattern;
    /** Command length */
	short length;
} uart_header;

#define LABTOOL_PATTERN_HDR_LEN 4
#define CHECKSUM_LEN 4

#define  SDIO_OUTBUF_LEN	2048
#define UART_BUF_SIZE    2048

#define MLAN_TYPE_CMD   1
/** Command type: WLAN */
#define TYPE_WLAN  0x0002
/** Command type: BT */
#define TYPE_HCI   0x0003

#define MLAN_PACK_START
#define MLAN_PACK_END           __attribute__((packed))

typedef struct _uart_cb {	/* uart control block */
	int uart_fd;
	unsigned int crc32_table[256];

	unsigned char uart_buf[UART_BUF_SIZE];	/* uart buffer */

} uart_cb, *puart_cb;

/** Labtool command header */
typedef struct _cmd_header {
    /** Command Type */
	short type;
    /** Command Sub-type */
	short sub_type;
    /** Command length (header+payload) */
	short length;
    /** Command status */
	short status;
    /** reserved */
	int reserved;
} cmd_header;

/** HostCmd_DS_COMMAND */
typedef struct MLAN_PACK_START _HostCmd_DS_COMMAND
{
	/** Command Header : Command */
	t_u16 command;
	/** Command Header : Size */
	t_u16 size;
	/** Command Header : Sequence number */
	t_u16 seq_num;
	/** Command Header : Result */
	t_u16 result;
	/** Command Body */
} MLAN_PACK_END HostCmd_DS_COMMAND;

typedef MLAN_PACK_START struct _SDIOPkt
{
	t_u16 size;
	t_u16 pkttype;
	HostCmd_DS_COMMAND hostcmd;
} MLAN_PACK_END SDIOPkt;

#endif /** _UART_WIFI_BRIDGE_H */
