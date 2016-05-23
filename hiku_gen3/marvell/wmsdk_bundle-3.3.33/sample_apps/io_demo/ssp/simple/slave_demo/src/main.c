/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/**
 *
 * Summary:
 * Simple application which uses SSP driver as slave.
 *
 * Description:
 * This application uses APIs exposed by mdev layer of SSP driver.
 * In this application, 88MC200 is configured in slave mode
 * (using SPI protocol).
 * It receives data sent by SSP master on SSP bus and prints it on the console.
 *
 * Specification of SSP pins are:
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

/* Read data from SSP master after every 100msec */
static void ssp_slave_read(os_thread_arg_t data)
{
	wmprintf("ssp_slave_demo Application Started\r\n");

	int rv, len, i, cnt = 0, error_cnt = 0;
	uint8_t x;

	while (1) {
		cnt++;
		memset(read_data, 0, BUF_LEN);

		/* Read random number of bytes from SSP bus */
		len = rand() % BUF_LEN;
		rv = 0;
		while (rv != len)
			rv += ssp_drv_read(ssp, read_data + rv, len - rv);
		wmprintf("Read iteration %d: data_len = %d\n\r", cnt, len);

		for (i = 0; i < len ; i++) {
			if ((i % 16) == 0)
				wmprintf("\n\r");
			wmprintf("%02x ", read_data[i]);
		}
		if (len)
			wmprintf("\n\r\n\r");

		/* Verify received data */
		for (i = 0; i < len ; i++)
			if (read_data[i] != (x = rand())) {
				wmprintf("Rx Data i = %d Mismatch Exp = %x"
					" Act = %x error cnt = %d\r\n",
					i, x, read_data[i], ++error_cnt);
				/* while (1); */
			}

		if (!(cnt % 100))
			wmprintf("Error Count = %d\r\n", error_cnt);
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
	wmprintf("SPI_Demo application Started\r\n");

	/* Initialize SSP Driver */
	ssp_drv_init(SSP_ID);

	/* Set SPI clock frequency (Optional) */
	ssp_drv_set_clk(SSP_ID, 13000000);

	/*
	 * Set SPI software ringbuffer size (Optional).
	 * This buffer is allocated in driver.
	 */
	ssp_drv_rxbuf_size(SSP_ID, BUF_LEN);

	/* Configure SSP as SPI device. SSP0 is configured as slave
	 */
	ssp = ssp_drv_open(SSP_ID, SSP_FRAME_SPI, SSP_SLAVE,
			DMA_DISABLE, -1, 0);


	/* Create the application thread */

	return os_thread_create(&app_thread,	/* thread handle */
			"ssp_demo",	/* thread name */
			ssp_slave_read,	/* entry function */
			0,	/* argument */
			&app_stack,	/* stack */
			OS_PRIO_2);	/* priority - medium low */
}
