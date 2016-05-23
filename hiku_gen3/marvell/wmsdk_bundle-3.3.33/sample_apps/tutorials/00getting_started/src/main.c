/*
 *  Copyright (C) 2008-2016 Marvell International Ltd.
 *  All Rights Reserved.
 */
/* Tutorial 0: Getting Started
 *
 * The 'Hello World' application
 */
#include <wmstdio.h>

int main()
{
	wmstdio_init(UART0_ID, 0);
	wmprintf("Firmware Started\r\n");

	return 0;
}
