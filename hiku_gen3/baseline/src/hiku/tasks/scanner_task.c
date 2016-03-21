/*
 * Scanner Task
 *
 * Created: 3/18/2016 4:36:31 PM
 *  Author: Rajan Bala
 *  
 * This file will contain the scanner module related handling
 * hiku gen 3 hardware utilizes a camera module to capture and process barcodes
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
#include "hiku_utils.h"
#include "hiku-tasks.h"

static void scanner_command_task_handler(void *pvParameters);
static const uint8_t *const i2c_message = (uint8_t *) "\r\n I am in the I2C Task\r\n\r\n>";




void create_scanner_task(Twi *twi_base, uint16_t stack_depth_words, unsigned portBASE_TYPE task_priority, portBASE_TYPE set_asynchronous_api)
{

	/* Register the default CLI commands. */
	//vRegisterCLICommands();

	/* Create the USART CLI task. */
	xTaskCreate(	scanner_command_task_handler,			/* The task that implements the command console. */
	(const int8_t *const) "SCAN_TSK",	/* Text name assigned to the task.  This is just to assist debugging.  The kernel does not use this name itself. */
	stack_depth_words,					/* The size of the stack allocated to the task. */
	NULL,			/* The parameter is used to pass the already configured USART port into the task. */
	task_priority,						/* The priority allocated to the task. */
	NULL);	
}

static void scanner_command_task_handler(void *pvParameters)
{
	static uint8_t local_buffer[60];
	
	memset((void *) local_buffer, 0x00, sizeof(local_buffer));
	sprintf((char *) local_buffer,
	"Scanner Task Running\r\n\r\n");
	printConsole(local_buffer);
	
	for (;;)
	{
		// main gut of the task handling
		// TODO: move the camera module initialization and handling of data over here
		vTaskDelay(300);
	}
	
	memset((void *) local_buffer, 0x00, sizeof(local_buffer));
	sprintf((char *) local_buffer,
	"Scanner Task Exiting\r\n\r\n");
	printConsole(local_buffer);
}

portBASE_TYPE did_scanner_test_pass(void)
{
	return 0;
}