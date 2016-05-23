/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 *
 * Summary:
 * Description:
 * This project shows how to use UART1 with MDEV layer. UART1 is configured at
 * baud rate of 115200 and enabled as 8bit.
 * It implements basic functions to read and write data in different
 * modes and configurations using the uart-driver API's
 */
#include <wmstdio.h>
#include <wm_os.h>
#include <stdlib.h>
#include <mdev_uart.h>
#include <cli.h>
#include <wm_utils.h>
#include <stdint.h>
#include <string.h>
#include <wmtypes.h>

/* CONFIG_ENABLE_LOGS */
/* Uncomment this to enable logs */
#define CONFIG_ENABLE_LOGS 1
#ifdef CONFIG_ENABLE_LOGS
#define uart_dbg wmprintf
#else
#define uart_dbg(...)
#endif

/* uart-test cli arguments  */
typedef struct uart_test_struct {
	char uart_mode;
	char dma_mode;
	char parity;
	char flow_cntrl;
	char stop_bit;
	unsigned long baud_rate;
	unsigned long blk_size;
	unsigned long no_itr;
} uart_test_arg;

/*UART handle*/
mdev_t *uart;

/* Size of UART buffer */
#define BUFSIZE 10000

/* Buffer for read and write*/
char buf[BUFSIZE];

/* This is an entry point for the application.
   All application specific initializations are performed here. */

uart_test_arg cli_arg;

/* uart write function */
static void uart_write(void)
{
	int len, i, j;
	uart_dbg("UART Writing Started\r\n");

	i = 0;
	while (i < cli_arg.no_itr) {
		i++;
		/* Write random number of random bytes */
		len = cli_arg.blk_size;
		for (j = 0; j < len; j++)
			buf[j] = rand();

		/* Write data on UART bus */
		len = uart_drv_write(uart, (uint8_t *)buf, len);
		uart_dbg("Write iteration %d: data_len = %d\n\r", i, len);

		for (j = 0; j < len ; j++) {
			if ((j % 16) == 0)
				uart_dbg("\n\r");
			uart_dbg("%02x ", buf[j]);
		}
		if (len)
			uart_dbg("\n\r\n\r");

		os_thread_sleep(os_msec_to_ticks(100));
	}
}

/* UART read function */
void uart_read()
{
	int i = 0, total = 0, j, error_cnt = 0;
	uint8_t x;
	while (i < cli_arg.no_itr) {
		i++;
		while (total < cli_arg.blk_size) {
			total += uart_drv_read(uart, (uint8_t *)&buf + total,
			cli_arg.blk_size);
		}
		wmprintf("Read data is:\r\n");
		for (j = 0; j < cli_arg.blk_size; j++) {
			if ((j % 16) == 0)
				uart_dbg("\n\r");
			uart_dbg("%02x ", buf[j]);
		}
		/* Verify received data */
		for (j = 0; j < cli_arg.blk_size; j++)
			if (buf[j] != (x = rand())) {
				error_cnt++;
				uart_dbg("Rx Data j = %d Mismatch Exp = %x"
					" Act = %x error cnt = %d\r\n",
					j, x, buf[j], error_cnt);
				break;
			}
		if (error_cnt != 0)
			wmprintf("Failure\r\n");
		else
			wmprintf("\r\nSuccess\r\n");
		error_cnt = 0;
		total = 0;
	}
}

static void uart_demo_call()
{
	/* Initialize UART1 with 8bit. This will register UART1 MDEV handler */
	uart_drv_init(UART1_ID, UART_8BIT);

	/* Enable/Disable DMA on UART1 */
	if (cli_arg.dma_mode == 'D') {
		uart_drv_xfer_mode(UART1_ID, UART_DMA_ENABLE);
	}

	if (cli_arg.uart_mode == 'R') {
		/* Set internal rx ringbuffer size to 3K */
		uart_drv_rxbuf_size(UART1_ID, 1024 * 3);
		/* Set DMA block size */
		uart_drv_dma_rd_blk_size(UART1_ID, cli_arg.blk_size);
		if (cli_arg.dma_mode == 'I')
			uart_drv_blocking_read(UART1_ID, true);
	}
	/* Check the parity */
	uint32_t parity = UART_PARITY_NONE;
	if (cli_arg.parity == 'E') {
		parity = UART_PARITY_EVEN;
		wmprintf("Even Parity\r\n");
	} else if (cli_arg.parity == 'O') {
		parity = UART_PARITY_ODD;
		wmprintf("Odd Parity\r\n");
	}

	/* Check the flow control */
	uint32_t flow_cntrl = FLOW_CONTROL_NONE;
	if (cli_arg.flow_cntrl == 'H')
		flow_cntrl = FLOW_CONTROL_HW;
	else if (cli_arg.flow_cntrl == 'S')
		flow_cntrl = FLOW_CONTROL_SW;

	/* Check the stopbit */
	uint32_t stop_bit = UART_STOPBITS_1;
	if (cli_arg.stop_bit == '2')
		stop_bit = UART_STOPBITS_2;

	/* Set the above configured options for UART */
	uart_drv_set_opts(UART1_ID, parity, stop_bit, flow_cntrl);

	/* Open UART1 with configured baud rate. This will return mdev UART1
	 * handle */
	wmprintf("Baud Rate is %d \r\n", cli_arg.baud_rate);
	uart = uart_drv_open(UART1_ID, cli_arg.baud_rate);

	if (cli_arg.uart_mode == 'T')
		uart_write();
	else
		uart_read();

	memset(buf, 0, BUFSIZE - 1);
	uart_drv_close(uart);
	uart_drv_deinit(UART1_ID);
}

/* CLI */
/* uart-test T/R D/P O/E/N N/H/S 1/2 9600/19200/38400/57600/115200/230400 5 5 */

/* This function sets the input configurations for uart */
static void uart_demo_config(int argc, char *argv[])
{
	cli_arg.uart_mode = 'T';
	cli_arg.dma_mode = 'P';
	cli_arg.parity = 'N';
	cli_arg.baud_rate = 115200;
	cli_arg.blk_size = 8;
	cli_arg.no_itr = 1;
	cli_arg.flow_cntrl = 'N';
	cli_arg.stop_bit = '1';
	if (argc == 9) {
		cli_arg.uart_mode = *argv[1];
		cli_arg.dma_mode = *argv[2];
		cli_arg.parity = *argv[3];
		cli_arg.flow_cntrl = *argv[4];
		cli_arg.stop_bit = *argv[5];
		cli_arg.baud_rate = atoi(argv[6]);
		cli_arg.blk_size = atoi(argv[7]);
		cli_arg.no_itr = atoi(argv[8]);
	}
	uart_demo_call();
}

static struct cli_command commands[] = {
	{"uart-test", "[uart_mode]", uart_demo_config},
};

int uart_demo_cli_init(void)
{
	int i;
	for (i = 0; i < sizeof(commands)/sizeof(struct cli_command); i++) {
		if (cli_register_command(&commands[i]))
			return 1;
	}
	return 0;
}
