/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 * Summary:
 * This project shows how to use I2C with MDEV layer.
 * It implements basic functions to read and write data in different
 * modes and configurations using the i2c-driver API's
 *
 * Description:
 * Master and slave generate a seed based on os_time_ticks.
 * Master generates a random data based on this seed and sends it to slave.
 * Slave verifies this data by generating the randon data using the seed.
 * Then slave generates random data using the same seed and sends it to master.
 * Master verifies this data by generating the same data.
 * Master prints Success on completion of the above process.
 *
 */
/* CLI */
/* i2c-test M/S P/D 7/11 10/100/400 8 */
#include <wmstdio.h>
#include <wm_os.h>
#include <stdlib.h>
#include <mdev_i2c.h>
#include <cli.h>
#include <wm_utils.h>
#include <stdint.h>
#include <string.h>
#include <wmtypes.h>
#include <wmlog.h>

/* Uncomment this to enable debug logs */
/* #define CONFIG_I2C_TEST_DEBUG */

#define i2c_test_e(...)				\
	wmlog_e("i2c_test", ##__VA_ARGS__)
#define i2c_test_w(...)				\
	wmlog_w("i2c_test", ##__VA_ARGS__)

#ifdef CONFIG_I2C_TEST_DEBUG
#define i2c_test_d(...)				\
	wmlog("i2c_test", ##__VA_ARGS__)
#else
#define i2c_test_d(...)
#endif /* ! CONFIG_I2C_TEST_DEBUG */

/* i2c-test cli arguments */
typedef struct i2c_test_struct {
	char i2c_mode;
	char tran_mode;
	char dma_mode;
	uint8_t addr_mode;
	uint16_t speed_mode;
	unsigned long no_byte;
} i2c_test_arg;

/*I2C handle*/
static mdev_t *i2c;

/*------------------Macro Definitions ------------------*/
#define BUF_LEN 1000
#define I2C_SLV_ADDR 0xC8

/* Buffers for read and write over I2C */
uint8_t read_data[BUF_LEN];
uint8_t write_data[BUF_LEN];

i2c_test_arg cli_arg;

/* This function receives data from I2C bus */
static void i2c_read()
{
	int len = 0, i = 0, error_cnt = 0, k = 0;
	uint8_t x = 0;
	i2c_test_d("I2C Reader");
	while (k < cli_arg.no_byte) {
retry:
		len = i2c_drv_read(i2c, read_data + k, cli_arg.no_byte - k);
		if (!len) {
			goto retry;
		}
		k += len;
		os_thread_sleep(1);
		i2c_test_d("Read %d Bytes", k);
	}
	/* Print data on console */
#ifdef CONFIG_I2C_TEST_DEBUG
	for (i = 0; i < k; i++)
		i2c_test_d("%02x ", read_data[i]);
#endif
	/* Verify received data */
	for (i = 0; i < k; i++)
		if (read_data[i] != (x = rand())) {
			error_cnt++;
			i2c_test_d("Rx Data i = %d Mismatch Exp = %x"
				" Act = %x error cnt = %d",
				i, x, read_data[i], error_cnt);
		break;
		}
	if (error_cnt != 0)
		wmprintf("Failure\r\n ");
	else
	    wmprintf("Success\r\n");
}


/* This function writes data on I2C bus */
static void i2c_write()
{
	i2c_test_d("I2C writer");
	int i = 0, len = 0, k = 0;
	for (i = 0; i < cli_arg.no_byte; i++)
		write_data[i] = rand();
	i = 0;
	while (k < cli_arg.no_byte) {
		i++;
		len = i2c_drv_write(i2c, write_data + k, cli_arg.no_byte - k);
		i2c_test_d("Written %d Bytes", len);
#ifdef CONFIG_I2C_TEST_DEBUG
		int j;
		for (j = 0; j < cli_arg.no_byte ; j++)
			i2c_test_d("%02x ", write_data[j]);
#endif
		k += len;
		len = 0;
	}
}

/* I2C call with given configuration */
static void i2c_demo_call()
{
	/* Initialize I2C Driver */
	i2c_drv_init(I2C1_PORT);

	uint32_t i2c_flag = 0;
	/* Check for Master/Slave */
	if (cli_arg.i2c_mode == 'S')
		i2c_flag |= I2C_DEVICE_SLAVE;
	/* Enable/Disable DMA */
	if (cli_arg.dma_mode == 'D')
		/* Feature not complete in the driver */
		;
	/* Address Bits */
	if (cli_arg.addr_mode == 11)
		i2c_flag |= I2C_10_BIT_ADDR;

	/* Speed of operation */
	if (cli_arg.speed_mode == 10)
		i2c_flag |= I2C_CLK_10KHZ;
	else if (cli_arg.speed_mode == 100)
		i2c_flag |= I2C_CLK_100KHZ;
	else if (cli_arg.speed_mode == 400)
		i2c_flag |= I2C_CLK_400KHZ;

	/* Set the I2C Address */
	i2c_flag |= I2C_SLAVEADR(I2C_SLV_ADDR >> 1);

	i2c = i2c_drv_open(I2C1_PORT, i2c_flag);

	if (cli_arg.i2c_mode == 'S') {
		i2c_read();
		i2c_write();
	} else {
		i2c_write();
		i2c_read();
	}

	/* Close the I2C driver */
	i2c_drv_close(i2c);
	i2c_drv_deinit(I2C1_PORT);
}


/* This function sets the input configurations for ssp */
static void i2c_demo_config(int argc, char *argv[])
{
	/* Default configurations */
	cli_arg.i2c_mode = 'M';
	cli_arg.dma_mode = 'P';
	cli_arg.addr_mode = 10;
	cli_arg.speed_mode = 100;
	cli_arg.no_byte = 8;
	/* Set the congfigurations according to the user */
	if (argc == 6) {
		cli_arg.i2c_mode = *argv[1];
		cli_arg.dma_mode = *argv[2];
		cli_arg.addr_mode = atoi(argv[3]);
		cli_arg.speed_mode = atoi(argv[4]);
		cli_arg.no_byte = atoi(argv[5]);
	}
	i2c_demo_call();
}

static struct cli_command commands[] = {
	{"i2c-test", "[i2c_mode]", i2c_demo_config},
};

int i2c_demo_cli_init(void)
{
	int i;
	for (i = 0; i < sizeof(commands)/sizeof(struct cli_command); i++) {
		if (cli_register_command(&commands[i]))
			return 1;
	}
	return 0;
}
