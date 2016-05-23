/*
 * Copyright 2008-2015 Marvell Technology Group Ltd.
 */

#include <stdint.h>
/* BOOTROM constrained constants */
#define STANDARD_HEADER_LEN 16
#define MAX_DATA_CHUNK 508
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) < (b)) ? (b) : (a))

/* Host specific parameters */
#define MAX_BUF_SIZE 500
/* Size of linebuf: used to uart_read a single line form ihex file */
#define LINE_BUF_SIZE 16
/* Used to store a command buffer to host system */
#define SYS_CMD_SIZE 150

#define GET_BYTE_1(a) ((a) & 0xff)
#define GET_BYTE_2(a) (((a) & (0xff00)) >> 8)
#define GET_BYTE_3(a) (((a) & 0xff0000) >> 16)
#define GET_BYTE_4(a) (((a) & 0xff000000) >> 24)
/* Device commands specific to UART BOOT Protocol*/
#define DVC_DETECT_CMD 0x55
#define CMD_ACK	0x53
#define CMD_ERASE_FLASH 0x51
#define DEV_RESP_SIZE 5
#define RX_BUF_SIZE 20
#define DEV_ADDR_SIZE 4
#define PACKET_RESP_SIZE 4
#define DVC_ACK_POS 0xac
#define DVC_ACK_NEG 0xa3
#define DVC_SECURITY_FRAME_SZ 5
/* In header packet data length is stored
 * at an array position 8 and has a length of 4 bytes
 */
#define DATA_LEN_SIZE	4
#define SECURITY_MODE_EN_CODE 0x5e
#define NO_CRC_HEADER_CODE 0x02
#define NO_CRC_ENTRY_ADDR_CODE 0x08
/* Stucture of header packet */
typedef struct {
	char type; /* Header Packet without CRC check */
	char crc_mode; /* CRC mode if used */
	char reserved[2]; /* Reserved */
	char address[DEV_ADDR_SIZE]; /* Entry Address */
	char len[DATA_LEN_SIZE]; /* LEN of data packet; LEN = 0
				for entry packet */
	char crc[DATA_LEN_SIZE];
} header_t;

typedef struct {
	char type;
	char passwd[4];
} passwd_t;

typedef struct {
	char *ihex_file;
	long passkey;
	char *port_id;
	uint8_t erase_flag;
	uint8_t pass_flag;
} param_t;

/* DEBUG LOGS */
#ifdef UARTBOOT_DEBUG
#define UART_DBG(...)				\
	do {					\
		printf("U_BOOT ");	\
		printf(__VA_ARGS__);	\
		printf("\n\r");		\
	} while (0);

#else
#define UART_DBG(...)
#endif

/* Error LOGS */
#define UART_ERROR_LOGS
#ifdef UART_ERROR_LOGS
#define UART_ERROR(...)		\
	printf(__VA_ARGS__);
#else
#define UART_ERROR(...)
#endif

/* Informative LOGS *
 * These are used for user to know the device progress
 */
#define uart_print(...)		\
	printf(__VA_ARGS__);

/* Host specific serial port calls */
#define uart_read(...)		\
	read(__VA_ARGS__);
#define uart_write(...)		\
	write(__VA_ARGS__);
#define uart_mem_alloc(...)	\
	malloc(__VA_ARGS__);

#define uart_file_open(...)	\
	open(__VA_ARGS__);

#define uart_file_close(...)	\
	close(__VA_ARGS__);
#define uart_mem_free(...)	\
	free(__VA_ARGS__);
