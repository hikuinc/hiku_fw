/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 * This file implements basic functions to read and write data in different
 * modes and configurations using the ssp-driver API's
 */
#include <wmstdio.h>
#include <wm_os.h>
#include <stdlib.h>
#include <mdev_ssp.h>
#include <cli.h>
#include <string.h>
#include <wm_utils.h>
#include <stdint.h>
#ifdef CONFIG_ENABLE_MCU_PM3
#define MAX_BUFFER_LENGTH (8 * 1024)
#else
#define MAX_BUFFER_LENGTH (60 * 1024)
#endif


#define RX_BUFFER_SIZE 8000

/* SSP port to use */
#ifdef CONFIG_CPU_MC200
#define SSP_ID SSP0_ID
#else
#define SSP_ID SSP1_ID
#endif

/* CONFIG_ENABLE_LOGS */
/* Uncomment this to enable logs */
/* #define CONFIG_ENABLE_LOGS 1 */
#ifdef CONFIG_ENABLE_LOGS
#define ssp_dbg wmprintf
#else
#define ssp_dbg(...)
#endif

/* ssp-test cli arguments  */
typedef struct ssp_test_struct {
	char ssp_mode;
	int ssp_node;
	int dma_status;
	int data_width;
	long buffer_length;
	long no_itr;
} ssp_test_arg;

/*SSP handle*/
mdev_t *ssp;

ssp_test_arg cli_arg;
static int error_cnt;
uint8_t read_data[MAX_BUFFER_LENGTH];
uint8_t write_data[MAX_BUFFER_LENGTH];

/*ssp-write function for half duplex communication*/
static void ssp_write(void)
{
	int len, i, j;
	ssp_dbg("SSP Writing Started\r\n");

	i = 0;
	while (i < cli_arg.no_itr) {
		i++;
		/* Write random number of random bytes */
		len = rand() % cli_arg.buffer_length;
		for (j = 0; j < len; j++)
			write_data[j] = rand();

		/* Write data on SSP bus */
		len = ssp_drv_write(ssp, write_data, NULL, len, 0);
		ssp_dbg("Write iteration %d: data_len = %d\n\r", i, len);

		for (j = 0; j < len ; j++) {
			if ((j % 16) == 0)
				ssp_dbg("\n\r");
			ssp_dbg("%02x ", write_data[j]);
		}
		if (len)
			ssp_dbg("\n\r\n\r");

		os_thread_sleep(os_msec_to_ticks(100));
	}
}


/*ssp-read function for half-duplex communication*/
static void ssp_read(void)
{
	ssp_dbg("SSP Reading Started\r\n");

	int rv, len, i, cnt = 0, error_cnt = 0, ret = 0;
	uint8_t x;
	while (cnt < cli_arg.no_itr) {
		cnt++;
		memset(read_data, 0, cli_arg.buffer_length);

		/* Read random number of bytes from SSP bus */
		len = rand() % cli_arg.buffer_length;
		rv = 0;
		while (rv != len) {
			ret = ssp_drv_read(ssp, read_data + rv, len - rv);
			if (ret > 0)
				rv += ret;
		}
		ssp_dbg("Read iteration %d: data_len = %d\n\r", cnt, len);

		for (i = 0; i < len ; i++) {
			if ((i % 16) == 0)
				ssp_dbg("\n\r");
			ssp_dbg("%02x ", read_data[i]);
		}
		if (len)
			ssp_dbg("\n\r\n\r");

		/* Verify received data */
		for (i = 0; i < len ; i++)
			if (read_data[i] != (x = rand())) {
				error_cnt++;
				ssp_dbg("Rx Data i = %d Mismatch Exp = %x"
					" Act = %x error cnt = %d\r\n",
					i, x, read_data[i], error_cnt);
				break;
				/* while (1); */
			}
		if (!(cnt % 100))
			ssp_dbg("Error Count = %d\r\n", error_cnt);
	}
}

/*ssp-data send and receive function for full-duplex communication*/
static void data_send_rec()
{
	int i = 0, j = 0, len = 0;
	error_cnt = 0;
	ssp_dbg("SSP Writing and Reading Started in Fullduplex mode\r\n");
	while (i < cli_arg.no_itr) {
		i++;
		/* Read and write random number of random bytes */
		len = cli_arg.buffer_length;
		for (j = 0; j < len; j++)
			write_data[j] = rand();

		memset(read_data, 0, cli_arg.buffer_length);
		/*
		 * Write len bytes data on SPI interface and read len bytes
		 * data from SPI interface.
		 */

		if (cli_arg.ssp_mode == SSP_MASTER) {
			/* Slave side sleep causes synchronization problem
			 * with master.
			 * Let it block in ssp_drv_write instead
			 */
			os_thread_sleep(os_msec_to_ticks(1000));
		}

		len = ssp_drv_write(ssp, write_data, read_data, len, 1);
		ssp_dbg("Iteration %d: %d bytes written\n\r", i, len);
		for (j = 0; j < len; j++) {
			if ((j % 16) == 0)
				ssp_dbg("\n\r");
			ssp_dbg("%02x ", read_data[j]);
		}
		ssp_dbg("\n\r\n\r");
		/* Verify received data */
		for (j = 0; j < len; j++)
			if (read_data[j] != write_data[j]) {
				ssp_dbg("Rx Data i = %d Mismatch Exp = %x"
					" Act = %x error cnt = %d\r\n",
					i, write_data[j], read_data[j],
					error_cnt);
				error_cnt++;
				return;
				/* while (1); */
			}

		if (!(i % 100))
			ssp_dbg("Error Count = %d\r\n", error_cnt);
	}
}

static void ssp_demo_call()
{
	/* Initialize SSP Driver */
	ssp_drv_init(SSP_ID);
	ssp_dbg("Initialised \r\n");
	/* Set SPI clock frequency (Optional) */
	ssp_drv_set_clk(SSP_ID, 13000000);
	ssp_drv_rxbuf_size(SSP_ID, RX_BUFFER_SIZE);
	/* Set SPI datasize */
	ssp_drv_datasize(SSP_ID, cli_arg.data_width);
	ssp_dbg("Opening SSP Driver \r\n");
	ssp = ssp_drv_open(SSP_ID,
			SSP_FRAME_SPI,
			cli_arg.ssp_node,
			cli_arg.dma_status,
			-1,
			0);
	switch (cli_arg.ssp_mode) {
	case 'f':
		ssp_dbg("Full Duplex Mode \r\n");
		data_send_rec();
		break;
	case 'r':
		ssp_dbg("Half Duplex Mode \r\n");
		ssp_read();
		break;
	case 'w':
		ssp_dbg("Half Duplex Mode \r\n");
		ssp_write();
		break;
	}
	if (error_cnt == 0)
		wmprintf("SUCCESS\r\n");
	else
		wmprintf("FAILURE\r\n");
	ssp_drv_close(ssp);
	ssp_drv_deinit(SSP_ID);
}
/*This function opens the ssp-driver with given configurations*/

/* CLI */
/* ssp-demo f M D 8 5 5 */
/* This function sets the input configurations for ssp */
static void ssp_demo_config(int argc, char *argv[])
{
	cli_arg.ssp_mode = 'f';
	cli_arg.ssp_node = SSP_SLAVE;
	cli_arg.dma_status = DMA_DISABLE;
	cli_arg.data_width = SSP_DATASIZE_8;
	cli_arg.buffer_length = 8;
	cli_arg.no_itr = 1;
	if (argc == 7) {
		cli_arg.ssp_mode = *argv[1];
		if ((*argv[2]) == 'S')
			cli_arg.ssp_node = SSP_SLAVE;
		else
			cli_arg.ssp_node = SSP_MASTER;
		if ((*argv[3]) == 'P')
			cli_arg.dma_status = DMA_DISABLE;
		else
			cli_arg.dma_status = DMA_ENABLE;
	cli_arg.data_width = atoi(argv[4]);
	switch (cli_arg.data_width) {
	case 8:
		cli_arg.data_width = SSP_DATASIZE_8;
		ssp_dbg("Datawidth is 8\r\n");
		break;
	case 16:
		cli_arg.data_width = SSP_DATASIZE_16;
		ssp_dbg("Datawidth is 16\r\n");
		break;
	case 18:
		cli_arg.data_width = SSP_DATASIZE_18;
		ssp_dbg("Datawidth is 18\r\n");
			break;
	case 32:
		cli_arg.data_width = SSP_DATASIZE_32;
		ssp_dbg("Datawidth is 32\r\n");
			break;
	default:
		cli_arg.data_width = SSP_DATASIZE_8;
		ssp_dbg("Datawidth is 8\r\n");
			break;
	}
	cli_arg.buffer_length = atoi(argv[5]);
	cli_arg.no_itr = atoi(argv[6]);
	}
	ssp_demo_call();
}

static struct cli_command commands[] = {
	{"ssp-test", "[ssp_mode]", ssp_demo_config},
};

int ssp_demo_cli_init(void)
{
	int i;
	for (i = 0; i < sizeof(commands)/sizeof(struct cli_command); i++) {
		if (cli_register_command(&commands[i]))
			return 1;
	}
	return 0;
}
