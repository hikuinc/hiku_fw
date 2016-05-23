/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/**
 *
 * Summary:
 * Simple Application which uses SSP driver for full duplex and half duplex
 * communication.
 * CLI is implemented for selecting various available configurations.
 *
 * ssp-test <f/w/r> <M/S> <D/M> <8/16/18/32> <No. of bytes> <No. of iterations>
 *
 * <f/w/r>
 * f : Full duplex communication (read and write)
 * w : Half duplex communication (write)
 * r : Half duplex communication (read)
 *
 * <M/S>
 * M : Master
 * S : Slave
 *
 * <P/D>
 * P : DMA Disable (IRQ mode)
 * E : DMA Enable (DMA mode)
 *
 * <8/16/18/32>
 * This field describes the datawidth during communication.
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
 * This application uses APIs exposed by mdev layer of SSP driver to demonstrate
 * full duplex and half duplex communication (using SPI protocol).
 *
 * Specification of SSP0 pins are:
 * Pin No(lk20-v3)  | Pin No(mc200_dev-v2) | Use |
 * -----------------------------------------------
 *   I32            |   IO32               | CLK |
 *   I33            |   IO33               | FRM |
 *   I34            |   IO34               | RX  |
 *   I35            |   IO35               | TX  |
 * -----------------------------------------------
 * Specification of SSP1 pins are:
 * Pin No(mw300-rd V2) | Use |
 * ---------------------------
 *   I11               | CLK |
 *   I12               | FRM |
 *   I14               | RX  |
 *   I13               | TX  |
 * ---------------------------
 */

#include <wmstdio.h>
#include <wm_os.h>
#include <stdlib.h>
#include <cli.h>

void ssp_demo_cli_init(void);

int main(void)
{
	/* Random seed. Since both master and slave use os_ticks_get()
	 * at the same time, both will use same seed and hence
	 * will generate same random numbers in exact same sequence */
	srand(os_ticks_get());
	/* Initialize wmstdio console */
	wmstdio_init(UART0_ID, 0);
	cli_init();
	wmprintf("SSP full duplex demo application started\r\n");
	ssp_demo_cli_init();
	wmprintf("cli init done\r\n");
	return 0;
}
