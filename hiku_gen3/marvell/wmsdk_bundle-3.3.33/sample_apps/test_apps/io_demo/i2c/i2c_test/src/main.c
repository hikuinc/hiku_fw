/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/**
 *
 * Summary:
 * Simple Application which uses I2C driver for I2C communication.
 * CLI is implemented for selecting various available configurations.
 *
 * Description:
 * Master and slave, both use os_time_ticks as seed for srand only once
 * at the start of main function.
 * Master then, generates and sends random data to slave.
 * Slave verifies this data by generating the randon data at its point.
 * Then slave continues to generate random data  and sends it to the master.
 * Master verifies this data by continuing random data generation at its point.
 * Master prints Success on completion of the above process.
 *
 * i2c-test
 *
 * <M/S>
 * M : Master
 * S : Slave
 * Master or Slave mode is specified.
 *
 * <P/D>
 * P : DMA Disable (IRQ mode)
 * D : DMA Enable (DMA mode)
 * DMA or IRQ mode is specified.
 *
 * <7/10>
 * 7 : 7 bit slave address
 * 10 : 10 bit slave address
 * 7 bit or 10 bit address is specified.
 *
 * <10/100/400>
 * 10 : 10kHz
 * 100 : 100kHz
 * 400 : 400kHz
 *
 * <No. of bytes>
 * This gives the number of bytes to be transfered in one iteration.
 *
 * Start the slave side first and then master side for proper synch.
 *
 * Description:
 * This application uses APIs exposed by mdev layer of i2c driver.
 *
 * Specification of I2C pins are:
 * Pin No(mw300-rd V2) | Use |
 *
 * -----------------------------
 *   I17        |     I2C1_SCL |
 *   I18        |     I2C1_SDA |
 * -----------------------------
 *
 * Connections:
 * Connect two boards(MW300) as:
 *
 * ----------------
 *   I17  ->  I17 |
 *   I18  ->  I18 |
 *   GND  ->  GND |
 * ----------------
 *
 */
/* CLI */
/* i2c-test M/S P/D 7/11 10/100/400 8 */

#include <wmstdio.h>
#include <wm_os.h>
#include <stdlib.h>
#include <cli.h>

void i2c_demo_cli_init(void);

/* It gives the usage */
static void cli_usage()
{
	wmprintf(""
		"i2c-test <M/S <P/D> <7/10> <10/100/400> <No. of bytes>\r\n");
	wmprintf(""
		"\n<M/S>\r\n"
		"M : Master\r\n"
		"S : Slave\r\n"
		"Master or Slave mode is specified.\r\n"
		"\n<P/D>\r\n"
		"P : DMA Disable (IRQ mode)\r\n");
	wmprintf(""
		"D : DMA Enable (DMA mode)\r\n"
		"DMA or IRQ mode is specified.\r\n"
		"\n<7/10>\r\n"
		"7 : 7 bit slave address\r\n"
		"10 : 10 bit slave address\r\n");
	wmprintf(""
		"7 bit or 10 bit address is specified\r\n"
		"\n<10/100/400>\r\n"
		"10 : 10kHz\r\n"
		"100 : 100kHz\r\n"
		"400 : 400kHz\r\n"
		"\n<No. of bytes>\r\n");
	wmprintf(""
	"This gives the number of bytes to be transfered in one "
	"iteration.\r\n");
}

int main(void)
{
	/* Random seed. Since both master and slave use os_ticks_get()
	 * at the same time, both will use same seed and hence
	 * will generate same random numbers in exact same sequence */
	srand(os_ticks_get());
	/* Initialize wmstdio console */
	wmstdio_init(UART0_ID, 0);
	cli_init();
	wmprintf("I2C Test App - CLI <options>\r\n");
	i2c_demo_cli_init();
	cli_usage();
	return 0;
}
