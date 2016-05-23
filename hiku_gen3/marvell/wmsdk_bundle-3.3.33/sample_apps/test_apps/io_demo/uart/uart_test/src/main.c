/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/**
 *
 * Summary:
 * Simple Application which uses UART driver for UART communication.
 * CLI is implemented for selecting various available configurations.
 *
 * uart-test
 *
 * <t/r>
 * t : Transmitter
 * r : Receiver
 * Transmitter or reciever mode is specified.
 *
 * <P/D>
 * P : DMA Disable (IRQ mode)
 * D : DMA Enable (DMA mode)
 * DMA or IRQ mode is specified.
 *
 * <O/E/N>
 * O : Odd Parity
 * E : Even Parity
 * N : None
 * Parity is specified.
 *
 * <H/S/N>
 * H : Hardware Flow control
 * S : Software Flow control
 * N : No Flow control
 *
 * <1/2>
 * 0 : No stopbit
 * 1 : Stop bit option UART_STOPBITS_1 is selected
 * 2 : Stop bit option UART_STOPBITS_1P5_2 is selected
 *
 * <9600/19200/38400/57600/115200/230400>
 * Specifies the Baud rate for UART communication
 *
 * <No. of bytes>
 * This gives the number of bytes to be transfered in one iteration.
 *
 * <No. of iterations>
 * This gives the no of iteration of bytes during communication.
 *
 * Start the slave side first and then master side for proper synch.
 *
 * Description:
 * This application uses APIs exposed by mdev layer of UART driver to
 * demonstrate full duplex and half duplex communication (using SPI protocol).
 *
 * Specification of UART1 pins are:
 * Pin No(mw300-rd V2) | Use |
 * ----------------------------
 *   I11               | CTS  |
 *   I12               | RTS  |
 *   I14               | RXD  |
 *   I13               | TXD  |
 * ----------------------------
 */

#include <wmstdio.h>
#include <wm_os.h>
#include <stdlib.h>
#include <cli.h>

void uart_demo_cli_init(void);

int main(void)
{
	/* Random seed. Since both master and slave use os_ticks_get()
	 * at the same time, both will use same seed and hence
	 * will generate same random numbers in exact same sequence */
	srand(os_ticks_get());
	/* Initialize wmstdio console */
	wmstdio_init(UART0_ID, 0);
	cli_init();
	wmprintf("UART demo application started\r\n");
	uart_demo_cli_init();
	wmprintf("cli init done\r\n");
	return 0;
}
