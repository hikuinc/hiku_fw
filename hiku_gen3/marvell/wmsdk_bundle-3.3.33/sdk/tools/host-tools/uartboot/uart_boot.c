/*
 * Boot from UART loader
 * Copyright 2008-2015 Marvell Technology Group Ltd.
 */

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#ifndef BUILD_MINGW
#include <termios.h>
#endif
#include <stdint.h>
#include "common.h"

/* entry_addr: Global to store the entry address of the code */
int entry_addr;
param_t param;

/* Size of buffer used to store code */
#define DBUF_SIZE MIN(MAX_BUF_SIZE, MAX_DATA_CHUNK)

/* fp: File pointer to the ihex file */
int fp;

/* Device commands specific to UART BOOT */
int usbdev;			/* handle to FTDI device */
static char response[RX_BUF_SIZE];	/* receive buffer */
static int bootrom_ver;
/* Parser Logic */
uint8_t read_hex_nibble(const uint8_t c)
{
	if ((c >= '0') && (c <= '9')) {
		return c - '0';
	} else if ((c >= 'A') && (c <= 'F')) {
		return c - 'A' + 10;
	} else if ((c >= 'a') && (c <= 'f')) {
		return c - 'a' + 10;
	} else {
		UART_ERROR("Error: char not in supported range");
		return 0;
	}
}

uint8_t read_hex_byte(const uint8_t *buf)
{
	uint8_t result;
	result = read_hex_nibble(buf[0]) << 4;
	result += read_hex_nibble(buf[1]);
	return result;
}

/* Loader Logic */
int host_send_ack()
{
	char ack_cmd = CMD_ACK;
	int ret = -1;
	while (ret != 1)
		ret = uart_write(usbdev, &ack_cmd, 1);
	return ret;
}

int send_entry_packet(int address)
{
	header_t entry_packet = {0};
	int ret = -1;

	/* no crc check entry header packet */
	entry_packet.type = NO_CRC_ENTRY_ADDR_CODE;

	/* copy entry address */
	memcpy(entry_packet.address, &address, DEV_ADDR_SIZE);

	ret = uart_write(usbdev, &entry_packet, sizeof(entry_packet));
	if (ret == -1)
		return ret;

	/* uart_read ack from device */
	memset(response, 0, RX_BUF_SIZE);
	ret = uart_read(usbdev, &response, DEV_RESP_SIZE);

	if (ret == -1)
		return ret;

	ret = host_send_ack();
	return ret;
}

int host_send_packet_header(int address, int len)
{
	header_t header_packet = {0};

	/* No CRC Packet */
	header_packet.type = NO_CRC_HEADER_CODE;

	/* set address */
	memcpy(header_packet.address, &address, DEV_ADDR_SIZE);

	/* set length of data in bytes */
	memcpy(header_packet.len, &len, DATA_LEN_SIZE);

	int ret = uart_write(usbdev, &header_packet, sizeof(header_packet));

	return ret;
}

int host_send_packet(uint8_t *address, int len)
{
	int ret = -1;
	ret = uart_write(usbdev, address, len);
	if (ret == -1)
		return ret;

	memset(response, 0, RX_BUF_SIZE);
	ret = uart_read(usbdev, &response, PACKET_RESP_SIZE);
	return ret;
}

int ramload_data(uint32_t addr, uint8_t *data, int seg_len)
{
	UART_DBG("Packet addres is %x", addr);
	int ret = -1;
	ret = host_send_packet_header(addr, seg_len);
	if (ret == -1) {
		UART_ERROR("uart_write packet header error\r\n");
		return ret;
	}
	/* Send ACK for packet header */
	ret = host_send_ack();

	if (seg_len < MAX_DATA_CHUNK)
		ret = host_send_packet(data, seg_len);
	else
		ret = host_send_packet(data, MAX_DATA_CHUNK);
	if (ret == -1) {
		UART_ERROR("Uart send text packet");
		return ret;
	}
	ret = host_send_ack();
	return ret;
}

int parse_hex_file(const char *fname)
{
	/* allocate a buffer to store data to be loaded into RAM
	 * MAX_BUF_SIZE is limited to MAX_DATA_CHUNK by BOOTROM constraint */
	uint8_t *dbuf = uart_mem_alloc(sizeof(char) * DBUF_SIZE);
	if (dbuf == NULL) {
		UART_DBG("failed to allocate data buffer");
		return -1;
	}
	/* Parameters required to uart_read & load hex data */
	int seg_count = 0;
	int bytes_uart_read = 0;
	uint32_t baseaddr = 0;
	uint32_t addr = 0;
	uint8_t len = 0;
	uint8_t type, checksum;
	int bytes_sent = 0;
	uint16_t off_addr;
	int ret, i;

	int linenum = 1;
	char c = '\t';
	/*  do this for each line */
	int cnt = 0;
	char *linebuf = NULL;
	int tmp_sz = LINE_BUF_SIZE;
	linebuf = uart_mem_alloc(tmp_sz);
	if (linebuf == NULL) {
		UART_ERROR("Unable to allocate line buff\r\n");
		return -1;
	}

	while (1) {
		/* while EOF */
		c = '\0';
		do {
			ret = uart_read(fp, &c, 1);
			if (ret == 0) {
				UART_DBG("EOF file");
				return -1;
			}
			/* check if EOF */
			if (c == '\r')
				uart_read(fp, &c, 1);
			if (c == '\n')
				linenum++;
		} while (c != ':');
		char buf[8];
		memset(buf, 0, 8);

		/* uart_read 8 bytes of data */
		uart_read(fp, buf, 8);
		/* initialize checksum to 0 */
		checksum = 0;
		/* len: 1 byte */
		len = read_hex_byte((uint8_t *)buf);
		/* offset address: 4 bytes */
		off_addr =
		    (read_hex_byte((uint8_t *)&buf[2]) << 8) +
		    read_hex_byte((uint8_t *)&buf[4]);
		/* type: 1 byte */
		type = read_hex_byte((uint8_t *)&buf[6]);
		if (len != 0) {
			/* allocate a temp buffer */
			/* If len is greater than the pre-allocated block
			 * allocate a bigger chunk for linebuf */
			if (len > LINE_BUF_SIZE) {
				tmp_sz = len;
				/* Free the previous allocation */
				uart_mem_free(linebuf);
				linebuf = uart_mem_alloc(len);
			}
			checksum =
			    read_hex_byte((uint8_t *)buf) +
			    read_hex_byte((uint8_t *)buf + 2) +
			    read_hex_byte((uint8_t *)buf + 4) +
			    read_hex_byte((uint8_t *)buf + 6);
			int i = 0;
			for (i = 0; i < len; i++) {
				ret = uart_read(fp, buf, 2);
				if (ret <= 0) {
					UART_ERROR("ihex file error\r\n");
					/* On error free tmp buf and return */
					if (linebuf)
						uart_mem_free(linebuf);
					return ret;
				}
				linebuf[i] = read_hex_byte((uint8_t *)buf);
				checksum += linebuf[i];

			}
			ret = uart_read(fp, buf, 2);
			checksum += read_hex_byte((uint8_t *)buf);
		}
		if (checksum) {
			UART_ERROR("checksum error at line %d", linenum);
			if (linebuf) {
				/* Free allocated buffers before return */
				uart_mem_free(linebuf);
				uart_mem_free(dbuf);
			}
			return -1;
		}

		switch (type) {
		case 0:
			/* data frame */
			if (len != 0) {
				if (bytes_uart_read == 0) {
					/* This is the first frame of a segment
					 */
					/* calculate address */
					addr = baseaddr + off_addr;
					UART_DBG("Start address for segment %d"
						 " is %x", seg_count, baseaddr);
				}
				/* check if dbuf has enough space */
				if (bytes_uart_read + len <= DBUF_SIZE) {
					/* move data into dbuf */
					memcpy(dbuf + bytes_uart_read, linebuf,
						len);
					bytes_uart_read += len;
					UART_DBG("linenum %d, bytes read "
						"%d", linenum, bytes_uart_read);
					memset(linebuf, 0, tmp_sz);

				} else {
					/* send the data */
					int ret = ramload_data(addr, dbuf,
							       bytes_uart_read);
					if (ret == -1) {
						UART_ERROR("Error while loading"
							"data\r\n");
						if (linebuf) {
							uart_mem_free(linebuf);
							uart_mem_free(dbuf);
						}
						return -1;
					}
					/* Update bytes_sent */
					bytes_sent = bytes_uart_read;
					/* Update new address */
					addr = addr + bytes_sent;
					/* reset the data buffer */
					memset(dbuf, 0, DBUF_SIZE);
					/* copy the remaining data into dbuf */
					memcpy(dbuf, linebuf, len);
					bytes_uart_read = len;
					/* Reset linebuf */
					memset(linebuf, 0, tmp_sz);
				}
				break;
			}
		case 1:
			UART_DBG("End of hex file\r\n");
			/* eof file */
			return 1;

		case 4:
			if (seg_count) {
				/* This is not the first segment
				   send out the previous data */
				ret = ramload_data(addr, dbuf, bytes_uart_read);
				if (ret == -1) {
					UART_ERROR("Error while loading"
						   "data\r\n");
					if (linebuf) {
						uart_mem_free(linebuf);
						uart_mem_free(dbuf);
					}
					return -1;
				}

				bytes_sent = bytes_uart_read;
				/* reset offset address */
				addr = 0;
				/* reset data buffer */
				memset(dbuf, 0, DBUF_SIZE);
			}
			seg_count++;
			baseaddr = (linebuf[0] << 24) + (linebuf[1] << 16);
			UART_DBG("Base address  of seg %d is %x", seg_count,
				 baseaddr);
			/* reset bytes_uart_read and linebuf */
			bytes_uart_read = 0;
			memset(linebuf, 0, tmp_sz);
			break;

		case 5:
			/* Type of data has changed; load the data
			 * that is already read from hex file but
			 * not loaded into RAM
			 */
			ret = ramload_data(addr, dbuf, bytes_uart_read);
			if (ret == -1) {
				UART_ERROR("Error while loading data\r\n");
				if (linebuf) {
					uart_mem_free(linebuf);
					uart_mem_free(dbuf);
				}
				return -1;
			}
			/* Update bytes_sent */
			bytes_sent = bytes_uart_read;
			/* reset offset address */
			addr = 0;
			/* reset data buffer */
			memset(dbuf, 0, DBUF_SIZE);

			if (len == 4) {
				entry_addr = (linebuf[0] << 24) +
				    (linebuf[1] << 16) + (linebuf[2] << 8) +
				    linebuf[3];

				/* Reset linebuf */
				memset(linebuf, 0, tmp_sz);
				UART_DBG("entry address is %x", entry_addr);
				ret = send_entry_packet(entry_addr);
				if (ret == -1) {
					UART_ERROR("Failed to set entry "
						"address\r\n");
					if (dbuf)
						uart_mem_free(dbuf);
					if (linebuf)
						uart_mem_free(linebuf);
					return ret;
				}

			} else {
				UART_ERROR("type 4 data format mismatch\r\n");
				break;
			}
			break;
		default:
			UART_ERROR("Unsupported data type %d\r\n", type);
			return -1;
		}
	}
	/* Free linebuf and dbuf */
	if (linebuf)
		uart_mem_free(linebuf);
	if (dbuf)
		uart_mem_free(dbuf);
	/* close hex file */
	uart_file_close(fp);
}

int die_usage(const char *argv0)
{
	UART_ERROR("Usage: %s -e(only for erase flash) -f"
		"<input_file(ihex format)> -p <Serial port enumeration id>"
		" -s <password(if SECURITY_MODE is enabled)>\n", argv0);
	exit(1);
}

int setup_host_serial_port(const char *port_id)
{
	/* system call using stty for 115200 Baud 8N1:
	 * CS8: 8n1 (8bit,no parity,1 stopbit
	 * -icanon min 1 time 1: read timeout 0.1 sec
	 * minimum 1 character read is mandatory
	 */
	char cmd[SYS_CMD_SIZE];
	snprintf(cmd, SYS_CMD_SIZE, "stty -F %s 115200 cs8 "
		"-icanon min 1 time 1", port_id);
	system(cmd);

	/* Open serial port */

	usbdev = uart_file_open(port_id, O_RDWR);
	if (usbdev < 0) {
		return -1;
	}
	UART_DBG("Serial port init done");
	return 0;

}

void erase_flash()
{
	uart_print("erasing flash...\r\n");
	char erase = CMD_ERASE_FLASH;
	int ret = uart_write(usbdev, &erase, 1);
	ret = -1;
	while (ret == -1)
		ret = uart_read(usbdev, response, 1)
	if ((response[0] & 0xff) == DVC_ACK_POS) {
		uart_print("flash erased\r\n");
	} else if ((response[0] & 0xff) == DVC_ACK_NEG) {
		UART_ERROR("Error while erasing flash\r\n");
	}
}

int main(int argc, char *argv[])
{
	if (argc == 1)
		die_usage(argv[0]);
	opterr = 0;
	int c;
	uint8_t port_flag = 0;
	param.pass_flag = 0;
	while ((c = getopt(argc, argv, "ep:s:f:")) != -1) {
		switch (c) {
		case 'p':
			port_flag = 1;
			param.port_id = optarg;
			break;
		case 's':
			param.pass_flag = 1;
			param.passkey = strtol(optarg,  NULL, 0);
			break;
		case 'f':
			param.ihex_file = optarg;
			break;
		case 'e':
			param.erase_flag = 1;
			break;
		default:
			goto end;
		}
	}
	if (port_flag == 0) {
		UART_ERROR("Please specify serial"
				"port enumeration id\r\n");
		return -1;
	}

	int ret = setup_host_serial_port(param.port_id);


	if (ret == -1) {
		UART_ERROR("failed to open serial port\r\n");
		return ret;
	}
	/* Send device detection command from HOST */
	ret = -1;
	const char dvc_detect = DVC_DETECT_CMD;
	while (ret == -1) {
		ret = uart_write(usbdev, &dvc_detect, 1);
	}
	UART_DBG("Device detected");

	/* Read device response to detection command.
	 * This response contains the BOOTROM_VERSION
	 */
	memset(response, 0, RX_BUF_SIZE);
	ret = -1;
	while (ret == -1) {
		ret = uart_read(usbdev, response, DEV_RESP_SIZE);
#ifdef BUILD_MINGW
		/* If uart_read fails, wait before next retry */
		volatile int tmp = 100000;
		while (tmp) {
			tmp--;
		}
#else
		sleep(1);
#endif /* BUILD_MINGW */
	}
	UART_DBG("Device response: (%d bytes): ", ret);

	/* Initial 4 bytes indicate BOOTROM version */
	bootrom_ver = ((int)response[3] & 0xff) << 24;
	bootrom_ver += ((int)response[2] & 0xff) << 16;
	bootrom_ver += ((int)response[1] & 0xff) << 8;
	bootrom_ver += ((int)response[0] & 0xff);
	uart_print("BOOTROM_VERSION %8x\r\n", bootrom_ver);

	/* Check if device SECURITY_MODE is enabled */
	if ((response[4] & 0xff) == DVC_ACK_POS) {
		uart_print("SECURITY_MODE is enabled\r\n");
		/* Erase flash */
		if (param.erase_flag == 1) {
			erase_flash();
			goto end;
		} else {
			/* Verify if password is specified */
			if (param.pass_flag == 0) {
				UART_ERROR
				    ("Please specify device password\r\n");
				return -1;
			}
		}
		passwd_t pass;
		pass.type = SECURITY_MODE_EN_CODE;
		pass.passwd[0] = GET_BYTE_1(param.passkey);
		pass.passwd[1] = GET_BYTE_2(param.passkey);
		pass.passwd[2] = GET_BYTE_3(param.passkey);
		pass.passwd[3] = GET_BYTE_4(param.passkey);
		ret = -1;
		while (ret == -1) {
			ret = uart_write(usbdev, &pass, DVC_SECURITY_FRAME_SZ);
		}
		ret = -1;
		while (ret == -1)
			ret = uart_read(usbdev, response, 1)

		if ((response[0] & 0xff) == DVC_ACK_POS) {
			uart_print("Verified PASSWORD\r\n");

		} else if ((response[0] & 0xff) == DVC_ACK_NEG) {
			UART_ERROR("Incorrect PASSWORD\r\n");
		    return -1;
		} else {
			UART_ERROR("Device not responding\r\n");
			return -1;
		}
		ret = uart_read(usbdev, response, DEV_RESP_SIZE);
	} else if ((response[4] & 0xff) == DVC_ACK_NEG) {
		uart_print("SECURITY_MODE disabled\r\n");
	}
	/* For now, we send out ACK even if the device
	* sends a response different from DEV_ACK_NEG or DEV_ACK_POS
	*/

	/* ACK to device response */
	ret = host_send_ack();
	if (ret != 1) {
		UART_DBG("device failed to send first ack");
		return ret;
	}

#ifndef BUILD_MINGW
	fp = uart_file_open(param.ihex_file, O_RDWR | O_NOCTTY);
#else
	fp = uart_file_open(param.ihex_file, O_RDWR);
#endif /* BUILD_MINGW */
	if (fp < 0) {
		UART_ERROR("ihex file does not exist\r\n");
		return -1;
	}

	/* Parse ihex file and load data into RAM */
	uart_print("In Progress...\r\n");
	ret = parse_hex_file(param.ihex_file);

	if (ret == -1) {
		uart_print("Error while loading data\r\n");
	} else {
		uart_print("Device is ready!!\r\n");
	}
end:
	/* close serial port */
	uart_file_close(usbdev);
	exit(0);
}
