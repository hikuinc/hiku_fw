/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/**
 *
 * Summary:
 * Simple Application which uses SSP driver for full duplex communication.
 * Start the slave side first and then master side for proper synch.
 *
 * Description:
 * This application uses APIs exposed by mdev layer of SSP driver to demonstrate
 * full duplex communication (using SPI protocol).
 * By default, SSP is configured in master mode. It can be configured as slave
 * by defining SSP_FULL_DUPLEX_SLAVE macro in the application.
 * The application sends 16 bytes data to other SPI device and reads 16 bytes
 * data after every 1000 msec and prints it on the console.
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
#include <mdev_ssp.h>

/* Buffer length */
#define BUF_LEN 4096

/* Uncomment this to compile slave side */
/* #define SSP_FULL_DUPLEX_SLAVE 1 */

/* Buffer to write data on SSP interface*/
uint8_t write_data[BUF_LEN];
/* Buffer to read data from SSP interface*/
uint8_t read_data[BUF_LEN];

/* SSP port to use */
#ifdef CONFIG_CPU_MC200
#define SSP_ID SSP0_ID
#else
#define SSP_ID SSP1_ID
#endif

/* SSP mdev handle */
mdev_t *ssp;

/**
 * All application specific initializations are performed here.
 */
int main(void)
{
	int len, i, j, error_cnt = 0;

	/* Random seed. Since both master and slave use os_ticks_get()
	 * at the same time, both will use same seed and hence
	 * will generate same random numbers in exact same sequence */
	srand(os_ticks_get());

	/* Initialize wmstdio console */
	wmstdio_init(UART0_ID, 0);
	wmprintf("SSP full duplex demo application started\r\n");

	/* Initialize SSP Driver */
	ssp_drv_init(SSP_ID);
	/* Set SPI clock frequency (Optional) */
	ssp_drv_set_clk(SSP_ID, 13000000);

#ifndef SSP_FULL_DUPLEX_SLAVE
	/*
	 * Configure SSP as SPI device in master mode if SSP_FULL_DUPLEX_SLAVE
	 * macro is not defined.
	 */
	ssp = ssp_drv_open(SSP_ID, SSP_FRAME_SPI, SSP_MASTER, DMA_DISABLE,
			-1, 0);
	wmprintf("SSP Master Started\r\n");
#else
	/*
	 * Set SPI software ringbuffer size (Optional).
	 * This buffer is allocated in driver.
	 */
	ssp_drv_rxbuf_size(SSP_ID, BUF_LEN);
	/*
	 * Configure SSP as SPI device in slave mode if SSP_FULL_DUPLEX_SLAVE
	 * macro is defined.
	 */
	ssp = ssp_drv_open(SSP_ID, SSP_FRAME_SPI, SSP_SLAVE, DMA_DISABLE,
			-1, 0);
	wmprintf("SSP Slave Started\r\n");
#endif

	i = 0;
	while (1) {
		i++;
		/* Read and write random number of random bytes */
		len = rand() % BUF_LEN;
		for (j = 0; j < len; j++)
			write_data[j] = rand();

		memset(read_data, 0, BUF_LEN);
		/*
		 * Write len bytes data on SPI interface and read len bytes
		 * data from SPI interface.
		 */
		len = ssp_drv_write(ssp, write_data, read_data, len, 1);
		wmprintf("Iteration %d: %d bytes written\n\r", i, len);
		for (j = 0; j < len; j++) {
			if ((j % 16) == 0)
				wmprintf("\n\r");
			wmprintf("%02x ", read_data[j]);
		}
		wmprintf("\n\r\n\r");

		for (j = 0; j < len; j++)
			if (read_data[j] != write_data[j]) {
				wmprintf("Rx Data i = %d Mismatch Exp = %x"
					" Act = %x error cnt = %d\r\n",
					i, write_data[j], read_data[j],
					++error_cnt);
				/* while (1); */
			}

		if (!(i % 100))
			wmprintf("Error Count = %d\r\n", error_cnt);

#ifndef SSP_FULL_DUPLEX_SLAVE
		/* Slave side sleep causes synchronization problem with master.
		 * Let it block in ssp_drv_write instead
		 */
		os_thread_sleep(os_msec_to_ticks(100));
#endif
	}
}
