/*
 * hiku_utils.c
 *
 * Created: 3/21/2016 4:24:35 PM
 *  Author: Rajan Bala
 */ 

#include "hiku_utils.h"

void printConsole(char *message)
{
	/* Cannot yet tell which CLI interface is in use, but both output functions
	guard check the port is initialised before it is used. */
#if (defined confINCLUDE_USART_CLI)
	usart_cli_output(message);
#endif	

#if (defined confINCLUDE_UART_CLI)
	uart_cli_output(message);
#endif
}