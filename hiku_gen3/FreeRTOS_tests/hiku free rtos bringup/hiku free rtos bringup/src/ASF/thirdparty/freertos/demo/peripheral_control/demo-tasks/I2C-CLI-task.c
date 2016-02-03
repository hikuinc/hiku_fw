/**
 *
 * \file
 *
 * \brief FreeRTOS+CLI task implementation example
 *
 * Copyright (c) 2012-2015 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */

/* Standard includes. */
#include <stdint.h>
#include <string.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* FreeRTOS+ includes. */
#include "FreeRTOS_CLI.h"

/* Atmel library includes. */
#include <freertos_twi_master.h>

/* Demo includes. */
#include "demo-tasks.h"



#ifdef HIKU_I2C_LED_DEBUG
#include "partest.h"
#include "timers.h"
/* Defines the LED toggled to provide visual feedback that the system is
 * running.  The rate is defined in milliseconds, then converted to RTOS ticks
 * by the portTICK_RATE_MS constant. */
#define I2C_TIMER_LED                  (1)
#define I2C_TIMER_RATE                 (500 / portTICK_RATE_MS)

/* Defines the LED that is turned on if an error is detected. */
#define I2C_ERROR_LED                           (1)

/* A block time of 0 ticks simply means "don't block". */
#define I2C_DONT_BLOCK                          (0)

/*
 * The callback function used by the software timer.  See the comments at the
 * top of this file.
 */
static void I2C_prvLEDTimerCallback(void *pvParameters);

#endif /* HIKU_I2C_LED_DEBUG */

#if (defined confINCLUDE_CDC_CLI) || (defined confINCLUDE_USART_CLI)

/* Dimensions the buffer into which input characters are placed. */
#define MAX_INPUT_SIZE          50

/* The size of the buffer provided to the USART driver for storage of received
 * bytes. */
#define RX_BUFFER_SIZE_BYTES    (50)

/* Baud rate to use. */
#define CLI_BAUD_RATE           115200

/*-----------------------------------------------------------*/

/*
 * The task that implements the command console processing.
 */
static void i2c_command_task(void *pvParameters);
static void printConsole(char *message);
static const uint8_t *const i2c_message = (uint8_t *) "\r\n I am in the I2C Task\r\n\r\n>";

typedef struct i2c_params 
{
	uint8_t chip_address;
	uint8_t reserved1;
	uint8_t chip_offset;
	uint8_t reserved2;
	freertos_twi_if twi_if;
}i2c_params_t;
/*-----------------------------------------------------------*/


/* The buffer provided to the USART driver to store incoming character in. */
static uint8_t receive_buffer[RX_BUFFER_SIZE_BYTES] = {0};

/* The USART instance used for input and output. */
static freertos_twi_if cli_twi_i2c = NULL;

/*-----------------------------------------------------------*/

void create_twi_i2c_test_task(Twi *twi_base, uint16_t stack_depth_words, unsigned portBASE_TYPE task_priority, portBASE_TYPE set_asynchronous_api)
{
	freertos_twi_if freertos_i2c;
	
	freertos_peripheral_options_t driver_options = {
		NULL,									/* The buffer used internally by the USART driver to store incoming characters. */
		0,							/* The size of the buffer provided to the USART driver to store incoming characters. */
		configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY,	/* The priority used by the USART interrupts. */
		TWI_I2C_MASTER,									/* Configure the USART for RS232 operation. */
		(USE_TX_ACCESS_MUTEX | USE_RX_ACCESS_MUTEX | WAIT_TX_COMPLETE | WAIT_RX_COMPLETE)		/* Use access mutex on Tx (as more than one task transmits) but not Rx. Wait for a Tx to complete before returning from send functions. */
	};

	/* Initialise the TWI interface. */
	freertos_i2c = freertos_twi_master_init(twi_base, &driver_options);
	configASSERT(freertos_i2c);
	twi_set_speed(twi_base, 400000, sysclk_get_cpu_hz());
	/* Register the default CLI commands. */
	//vRegisterCLICommands();
	
	i2c_params_t *prms = (i2c_params_t *)pvPortMalloc(sizeof(i2c_params_t));
	prms->chip_address = 0x4F;
	prms->chip_offset = 0x0;
	prms->twi_if = freertos_i2c;
	/* Create the USART CLI task. */
	xTaskCreate(	i2c_command_task,			/* The task that implements the command console. */
					(const int8_t *const) "I2C_CLI",	/* Text name assigned to the task.  This is just to assist debugging.  The kernel does not use this name itself. */
					stack_depth_words,					/* The size of the stack allocated to the task. */
					(void *) (prms),			/* The parameter is used to pass the already configured USART port into the task. */
					task_priority,						/* The priority allocated to the task. */
					NULL);								/* Used to store the handle to the created task - in this case the handle is not required. */
}

/*-----------------------------------------------------------*/

static void i2c_command_task(void *pvParameters)
{
	uint8_t received_char, input_index = 0,data=0, *output_string;
	static uint8_t local_buffer[60];
	static uint8_t data_buf[4];
	static int8_t input_string[MAX_INPUT_SIZE],
			last_input_string[MAX_INPUT_SIZE];
	portBASE_TYPE returned_value;
	portTickType max_block_time_ticks = 200UL / portTICK_RATE_MS;
	status_code_t status = STATUS_OK;
	const portTickType xDelay = 500;
	xTimerHandle xLEDTimer;
	
	i2c_params_t params = *(i2c_params_t *)pvParameters;
	
	memset((void *) local_buffer, 0x00, sizeof(local_buffer));
	sprintf((char *) local_buffer,
			"I2C Task Running (addr=0x%02x)\r\n\r\n",params.chip_address);
	printConsole(local_buffer);	
	
	uint8_t chipAddress =params.chip_address;
	twi_packet_t packet;
	packet.addr[0] = params.chip_offset;
	packet.addr_length = 1;
	packet.buffer = data_buf;
	packet.length = 4;
	packet.chip = 0x0;
	
	cli_twi_i2c = (freertos_twi_if) params.twi_if;
	configASSERT(cli_twi_i2c);	
#ifdef HIKU_I2C_LED_DEBUG	
	/* Create the timer that toggles an LED to show that the system is running,
	and that the other tasks are behaving as expected. */
	xLEDTimer = xTimerCreate((const signed char * const) "LED timer",/* A text name, purely to help debugging. */
							I2C_TIMER_RATE,	/* The timer period. */
							pdTRUE,						/* This is an auto-reload timer, so xAutoReload is set to pdTRUE. */
							NULL,						/* The timer does not use its ID, so the ID is just set to NULL. */
							I2C_prvLEDTimerCallback			/* The function that is called each time the timer expires. */
							);

	/* Sanity check the timer's creation, then start the timer.  The timer
	will not actually start until the FreeRTOS kernel is started. */
	configASSERT(xLEDTimer);
	xTimerStart(xLEDTimer, I2C_DONT_BLOCK);	

#endif /* HIKU_I2C_LED_DEBUG */	
	
	for (;;) {	
		
		packet.chip = chipAddress;
		memset((void *) local_buffer, 0x00, sizeof(local_buffer));
		sprintf((char *) local_buffer,"I2C Probe: addr=0x%x status:%ld\r\n\r\n",
			chipAddress,(twi_probe(cli_twi_i2c, chipAddress)));
		printConsole(local_buffer);			
		
		if((status=freertos_twi_read_packet(cli_twi_i2c, &packet, max_block_time_ticks)) != STATUS_OK)
		{
			memset((void *) local_buffer, 0x00, sizeof(local_buffer));
			sprintf((char *) local_buffer,"I2C Read Packet:addr=0x%x status:%ld\r\n\r\n",
					chipAddress,status);
			printConsole(local_buffer);	
		}
		else
		{
			memset((void *) local_buffer, 0x00, sizeof(local_buffer));
			sprintf((char *) local_buffer,"I2C Read Packet:addr=0x%x status:%ld, buff=0x%02x\r\n\r\n",
			chipAddress,status,*data_buf);
			printConsole(local_buffer);			
		}
		vTaskDelay(xDelay);
	}
	memset((void *) local_buffer, 0x00, sizeof(local_buffer));
	sprintf((char *) local_buffer,
	"I2C Task Exiting\r\n\r\n");
	printConsole(local_buffer);
	
	xTimerStop(xLEDTimer,I2C_DONT_BLOCK);
	vParTestSetLED(I2C_TIMER_LED, false);
	
}

static void printConsole(char *message)
{
	/* Cannot yet tell which CLI interface is in use, but both output functions
	guard check the port is initialised before it is used. */
#if (defined confINCLUDE_USART_CLI)
	usart_cli_output(message);
#endif		
}


static void I2C_prvLEDTimerCallback(void *pvParameters)
{
	portBASE_TYPE xStatus = pdPASS;

	/* Just to remove compiler warnings. */
	(void) pvParameters;

	/* Toggle an LED to show the system is executing. */
	vParTestToggleLED(I2C_TIMER_LED);
}

#endif
