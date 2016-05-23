/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/**
 *
 * Summary:
 * Simple Application which uses SSP driver as master.
 *
 * Description:
 * This application uses APIs exposed by mdev layer SSP driver.
 * In this application 88MC200 is configured in master mode
 * (using SPI protocol).
 * It sends 16 bytes data to SPI slave device after every 100 msec.
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

/* Thread handle */
static os_thread_t app_thread;

/* Buffer to be used as stack */
static os_thread_stack_define(app_stack, 4096);

/* Buffer to write data on SSP interface*/
uint8_t write_data[BUF_LEN];

/* SSP port to use */
#ifdef CONFIG_CPU_MC200
#define SSP_ID SSP0_ID
#else
#define SSP_ID SSP1_ID
#endif

/* SSP mdev handle */
mdev_t *ssp;

/* Write data from SSP master after every 100msec */
static void ssp_master_write(os_thread_arg_t data)
{
	int len, i, j;

	wmprintf("SSP Writer Application Started\r\n");

	i = 0;
	while (1) {
		i++;
		/* Write random number of random bytes */
		len = rand() % BUF_LEN;
		for (j = 0; j < len; j++)
			write_data[j] = rand();

		/* Write data on SSP bus */
		len = ssp_drv_write(ssp, write_data, NULL, len, 0);
		wmprintf("Write iteration %d: data_len = %d\n\r", i, len);

		for (j = 0; j < len ; j++) {
			if ((j % 16) == 0)
				wmprintf("\n\r");
			wmprintf("%02x ", write_data[j]);
		}
		if (len)
			wmprintf("\n\r\n\r");

		os_thread_sleep(os_msec_to_ticks(100));
	}
}

/**
 * All application specific initializations are performed here.
 */
int main(void)
{
	/* Random seed. Since both master and slave use os_ticks_get()
	 * at the same time, both will use same seed and hence
	 * will generate same random numbers in exact same sequence */
	srand(os_ticks_get());

	/* Initialize wmstdio console */
	wmstdio_init(UART0_ID, 0);
	wmprintf("ssp_master_demo application Started\r\n");

	/* Initialize SSP Driver */
	ssp_drv_init(SSP_ID);
	/* Set SPI clock frequency (Optional) */
	ssp_drv_set_clk(SSP_ID, 13000000);
	/* Configure SSP as SPI device. SSP0 is configured as master.
	 */
	ssp = ssp_drv_open(SSP_ID, SSP_FRAME_SPI, SSP_MASTER, DMA_DISABLE,
			-1, 0);

	/* Create the application thread */
	return os_thread_create(&app_thread,	/* thread handle */
			"spi_demo",	/* thread name */
			ssp_master_write,	/* entry function */
			0,	/* argument */
			&app_stack,	/* stack */
			OS_PRIO_2);	/* priority - medium low */
}
