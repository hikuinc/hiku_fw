/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Simple Application which uses I2C driver.
 *
 * Summary:
 * This application demonstrates use of 88MC200 I2C peripheral in slave mode.
 *
 * Description:
 *
 * This application uses APIs exposed by MDEV layer of I2C driver.
 * In this application 88MC200 is configured in slave mode. It acts as an
 * I2C SRAM. A complimentary master application is provided.
 *
 * This is sample application to demonstrate I2C mdev layer
 * as receiver in slave mode.
 * Specification of I2C1 pins are:
 *| Pin No(mc200) | Use     |
 *---------------------------
 *|   IO4         | SDA     |
 *|   IO5         | SCL     |
 * --------------------------
 * Connect IO4 and IO5 pin of transmitter to IO4 and IO5 pin
 * of receiver.
 */

#include <wmstdio.h>
#include <wm_os.h>
#include <mdev_i2c.h>

/*------------------Macro Definitions ------------------*/
#define wake1 "wakeup"
#define I2C_SLV_ADDR 0xC8

/*------------------Global Variable Definitions ---------*/
/* Thread handle */
static os_thread_t slave_sram_test;

/* Buffer to be used as stack */
static os_thread_stack_define(slave_stack, 4096);

#define log(_fmt_, ...)				\
	wmprintf(_fmt_"\n\r", ##__VA_ARGS__)

/* Message format */
typedef struct {
	/* ISR event */
	int evt;
	/* Not used */
	void *data;
} sm_msg_t;

/* Messages passed from ISR callback to thread context */
#define SM_MAX_MESSAGES 40

/* To pass messages to the state machine */
static os_queue_pool_define(sm_queue_data, SM_MAX_MESSAGES * sizeof(sm_msg_t));

/* Message queue to send and accept messages from ISR callback */
os_queue_t slave_q;

/* mdev handle */
mdev_t *i2c1;

#define SRAM_SIZE 4000
uint8_t sram_copy[SRAM_SIZE];
uint16_t sram_curr_offset = 0x0;

enum {
	MODE_ADDRESS_READ,
	MODE_DATA_READ
};

int curr_mode = MODE_ADDRESS_READ;

#define RD_BUF_SIZE 4
uint8_t read_buffer[RD_BUF_SIZE];
uint32_t rd_buff_offset;

/* Parse the address field of the incoming frame */
static void handle_address_mode(uint8_t byte)
{
	read_buffer[rd_buff_offset] = byte;
	rd_buff_offset++;

	if (rd_buff_offset == sizeof(sram_curr_offset)) {
		memcpy(&sram_curr_offset, read_buffer,
		       sizeof(sram_curr_offset));
		if (sram_curr_offset >= SRAM_SIZE)	/* Do not overrun */
			sram_curr_offset = SRAM_SIZE - 1;
		/* Address has been read */
		log("Address %d (0x%x)", sram_curr_offset, sram_curr_offset);
		curr_mode = MODE_DATA_READ;
		rd_buff_offset = 0;
	}
}

static void i2c_slave_sram(os_thread_arg_t data)
{
	uint8_t byte;
	int rv;
	log("SRAM simulator test device");

	/* Initialize main copy */
	memset(sram_copy, 0xFF, sizeof(sram_copy));
	while (1) {
		/*
		 * Wait till we get a message from I2C driver (routed
		 * through callback.
		 */
		sm_msg_t msg;
		rv = os_queue_recv(&slave_q, &msg, OS_WAIT_FOREVER);
		if (rv != WM_SUCCESS) {
			wmprintf("os_queue_recv failed\n\r");
			goto sram_error;
		}

		switch (msg.evt) {
		case I2C_INT_RD_REQ:
			/* wmprintf("W: %x\n\r",
			   sram_copy[sram_curr_offset]); */
			rv = i2c_drv_write(i2c1,
					   &sram_copy[sram_curr_offset], 1);
			if (rv != 1) {
				wmprintf("%s: Only %d bytes written\n\r",
					 __func__, rv);
				break;
			}

			sram_curr_offset += 1;
			break;
		case I2C_INT_RX_FULL:
			rv = i2c_drv_read(i2c1, &byte, 1);
			if (rv != 1) {
				wmprintf("%s: failed: %d\n\r", __func__, rv);
				break;
			}

			/* wmprintf("R: %x @ %x\n\r", byte, */
			/*       sram_curr_offset); */
			if (curr_mode == MODE_ADDRESS_READ) {
				handle_address_mode(byte);
			} else {
				/* Data mode */
				sram_copy[sram_curr_offset] = byte;
				sram_curr_offset =
				    (sram_curr_offset + 1) % SRAM_SIZE;
			}
			break;
		case I2C_INT_RX_DONE:
			/* wmprintf("I2C_INT_RX_DONE\n\r"); */
			curr_mode = MODE_ADDRESS_READ;
			break;
		case I2C_INT_STOP_DET:
			/* wmprintf("I2C_INT_STOP_DET\n\r"); */
			curr_mode = MODE_ADDRESS_READ;
			break;
		default:
			wmprintf("i2c slave unknown event: %d\n\r", msg.evt);
		}
	}

sram_error:
	log("SRAM slave error");
	while (1) {
		os_thread_sleep(os_msec_to_ticks(5000));
	}
}

/* This is called in ISR context. Keep this as small as possible */
static void i2c_cbfunc(I2C_INT_Type type, void *data)
{
	sm_msg_t msg;
	msg.evt = type;
	msg.data = data;

	/*
	 * We are in ISR context. Do not wait and do not check return
	 * value.
	 */
	os_queue_send(&slave_q, &msg, OS_NO_WAIT);
}

/**
* All application specific initializations are performed here.
*/
int main(void)
{
	wmstdio_init(UART0_ID, 0);
	wmprintf("I2C_Demo application Started\r\n");

	/* Queue to receive requests i2c callback */
	int rv = os_queue_create(&slave_q, "sm-q", sizeof(sm_msg_t),
				 &sm_queue_data);
	if (rv != WM_SUCCESS) {
		log("Unable to create msg queue");
		return rv;
	}

	/*-------- I2C driver specific calls -------*/
	/* Initialize I2C Driver */
	i2c_drv_init(I2C1_PORT);

	/* I2C1_PORT is configured as slave */
	i2c1 = i2c_drv_open(I2C1_PORT, I2C_DEVICE_SLAVE
			    | I2C_SLAVEADR(I2C_SLV_ADDR >> 1));
	rv = i2c_drv_set_callback(i2c1, i2c_cbfunc, NULL);

	/* Create the main application thread */
	os_thread_create(&slave_sram_test,	/* thread handle */
			 "i2c_demo_slave",	/* thread name */
			 i2c_slave_sram,	/* entry function */
			 0,	/* argument */
			 &slave_stack,	/* stack */
			 OS_PRIO_3);	/* priority - medium low */

	return 0;

}
