/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Simple Application which uses I2C driver.
 *
 * Summary:
 * This application demonstrates use of 88MC200 I2C peripheral in master mode.
 *
 * Description:
 * This application uses APIs exposed by MDEV layer of I2C driver.
 * In this application 88MC200 is configured in master mode. It interfaces
 * with an I2C SRAM. A complementary I2C slave application is provided
 * separately to interface two MC200 boards.
 *
 * This is sample application to demonstrate I2C mdev layer
 * as transmitter in master mode.
 * Specification of I2C1 pins (on lk20-v3)are:
 *
 *| Pin No(mc200) | Use     |
 *---------------------------
 *|   IO4         | SDA     |
 *|   IO5         | SCL     |
 * --------------------------
 * Connect IO4 and IO5 pin of transmitter to IO4 and IO5 pin
 * of receiver.
 *
 * @note: I2C DMA mode is supported for master write only.
 * DMA hardware for I2C restricts data transfer at one go
 * to 4000 bytes.
 *
 */

#include <wmstdio.h>
#include <wm_os.h>
#include <mdev_i2c.h>

#include <stdlib.h>

/*------------------Macro Definitions ------------------*/
#define wake1 "wakeup"
#define I2C_SLV_ADDR 0xC8

/*------------------Global Variable Definitions ---------*/
/* Thread handle */
static os_thread_t master_sram_test;

/* Buffer to be used as stack */
static os_thread_stack_define(master_stack, 4096);

#define log(_fmt_, ...)				\
	wmprintf(_fmt_"\n\r", ##__VA_ARGS__)

/* mdev handle */
mdev_t *i2c1;


#define SRAM_SIZE 4000

typedef enum {
	MODE_DMA = 0,
	MODE_PIO,
} mode;

static int master_write_to_sram(const uint8_t *buffer,
				int size, mode transf_mode)
{
	/*
	 * Write SRAM address offset. Observe that we do not actually
	 * write the SRAM I2C device address. That part is taken care of
	 * automatically by the I2C slave hardware on our chip
	 */
	int rv;
#if defined(CONFIG_CPU_MW300)
	if (transf_mode == MODE_DMA)
		rv =  i2c_drv_dma_write(i2c1, buffer, size);
	else
#endif
		rv = i2c_drv_write(i2c1, buffer, size);

	if (rv != size) {
		log("Master write to SRAM: address write failed");
		return -WM_FAIL;
	}

	rv = i2c_drv_wait_till_inactivity(i2c1, 20);
	if (rv != WM_SUCCESS) {
		log("%s: Wait till I2c inactivity failed", __func__);
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

static int master_read_from_sram(uint16_t sram_addr, uint8_t *buffer,
				int size, mode transf_mode)
{
	int rv;

	/*
	 * Write SRAM address offset. Observe that we do not actually
	 * write the SRAM I2C device address. That part is taken care of
	 * automatically by the I2C slave hardware on our chip
	 */
	rv = i2c_drv_write(i2c1, &sram_addr, sizeof(uint16_t));
	if (rv != sizeof(uint16_t)) {
		log("%s: Master failed to write to SRAM", __func__);
		return -WM_FAIL;
	}
#if defined(CONFIG_CPU_MW300)
	/* Read data */
	if (transf_mode == MODE_DMA)
		rv = i2c_drv_dma_read(i2c1, buffer, size);
	else
#endif
		rv = i2c_drv_read(i2c1, buffer, size);

	if (rv != size) {
		log("%s: Master failed to read from SRAM %d", __func__, size);
		return -WM_FAIL;
	}

	return 0;
}


/* This function reads data from I2C bus */
static void i2c_master_sram_test(int mode)
{
	int rv;
	log("SRAM simulator test user");

	{			/* Write and read back 1 byte */
		log("Simple test 1");
		uint8_t wdata = 0xAA;
		uint16_t addr = 0x0008;

		uint8_t buf[3];
		memcpy(buf, &addr, sizeof(addr));
		memcpy(&buf[sizeof(addr)], &wdata, sizeof(wdata));

		rv = master_write_to_sram(buf, sizeof(buf), mode);
		if (rv != 0)
			goto sram_error;

		uint8_t rdata;
		rv = master_read_from_sram(addr, &rdata, sizeof(rdata), mode);
		if (rv != 0)
			goto sram_error;

		if (wdata == rdata)
			log("SUCCESS");
		else {
			log("Failed: wdata: %x rdata: %x", wdata, rdata);
			goto sram_error;
		}
	}

	{
		/*
		 * Write and read back three bytes at 1 byte aligned location.
		 */
		log("Simple test 2");
		uint8_t wdata[] = { 0xAA, 0x55, 0x12 };
		uint16_t addr = 0x0001;

		uint8_t buf[sizeof(addr) + sizeof(wdata)];
		memcpy(buf, &addr, sizeof(addr));
		memcpy(&buf[sizeof(addr)], wdata, sizeof(wdata));

		int rv = master_write_to_sram(buf, sizeof(buf), mode);
		if (rv != 0)
			goto sram_error;

		uint8_t rdata[3];
		rv = master_read_from_sram(addr, rdata, sizeof(rdata), mode);
		if (rv != 0)
			goto sram_error;

		if (!memcmp(wdata, rdata, sizeof(rdata)))
			log("SUCCESS");
		else {
			goto sram_error;
		}
	}

	{
		/*
		 * Read and write random number of bytes at random location
		 * for random number of times.
		 */
		log("complex test 1");
		int iterations = 1000;
		while (iterations--) {
			wmprintf("Iter id %d\r\n", iterations);
			/* Random seed */
			srand(os_ticks_get());
			/* Select random address */
			uint16_t sram_addr = rand() % SRAM_SIZE;

			int write_count;
			/* Select random size */
			write_count = rand() % (SRAM_SIZE - sram_addr);
			if (write_count == 0)
				continue;
			log("Iter: %d To address: %x Size: %d",
			    iterations, sram_addr, write_count);

			/* Allocate buffer to read back data */
			uint8_t *rbuf = os_mem_alloc(write_count);
			if (!rbuf) {
				log("write buf alloc failed\n\r");
				/* Continue as we may have smaller size
				   test next time */
				continue;
			}

			/* Allocate buffer to generate data */
			uint8_t *wbuf = os_mem_alloc(sizeof(sram_addr) +
						     write_count);
			if (!wbuf) {
				log("read buf alloc failed\n\r");
				/* Continue as we may have smaller size
				   test next time */
				os_mem_free(rbuf);
				continue;
			}

			memcpy(wbuf, &sram_addr, sizeof(sram_addr));

			/* Generate random data */
			int i = 2; /* Skip the address area */
			for (; i < write_count; i++)
				wbuf[i] = rand() % 0xFF;

			/* Write to SRAM */
			int rv = master_write_to_sram(wbuf, sizeof(sram_addr) +
						      write_count, mode);
			if (rv != 0) {
				os_mem_free(rbuf);
				os_mem_free(wbuf);
				goto sram_error;
			}

			/* Read back data from same location and same size */
			rv = master_read_from_sram(sram_addr, rbuf,
						   write_count, mode);
			if (rv != 0) {
				os_mem_free(rbuf);
				os_mem_free(wbuf);
				goto sram_error;
			}

			/* Compare sent and received data */
			int fail_offset = memcmp(&wbuf[2], rbuf, write_count);
			if (!fail_offset) {
				os_mem_free(rbuf);
				os_mem_free(wbuf);
				log("SUCCESS");
			} else {
				log("Failed: %d", fail_offset);
				os_mem_free(rbuf);
				os_mem_free(wbuf);
				goto sram_error;
			}
		}
	}

	log("SRAM success");
	return;
sram_error:
	log("SRAM error");
	while (1) {
		os_thread_sleep(os_msec_to_ticks(5000));
	}
}

static void i2c_master_sram(os_thread_arg_t data)
{

#if defined(CONFIG_CPU_MW300)
	wmprintf("test mode: DMA\r\n");
	i2c_master_sram_test(MODE_DMA);
#endif
	wmprintf("test mode: PIO\r\n");
	i2c_master_sram_test(MODE_PIO);
	os_thread_self_complete(NULL);
}

/**
 * All application specific initializations are performed here.
 */
int main(void)
{
	wmstdio_init(UART0_ID, 0);
	wmprintf("I2C_Demo application Started\r\n");

	/*-------- I2C driver specific calls -------*/

	/* Initialize I2C Driver */
	i2c_drv_init(I2C1_PORT);

	/* I2C1 is configured as master */
	i2c1 = i2c_drv_open(I2C1_PORT, I2C_SLAVEADR(I2C_SLV_ADDR >> 1));

	os_thread_create(&master_sram_test,	/* thread handle */
			 "i2c_demo_master",	/* thread name */
			 i2c_master_sram,	/* entry function */
			 0,	/* argument */
			 &master_stack,	/* stack */
			 OS_PRIO_3);	/* priority - medium low */

	return 0;
}
