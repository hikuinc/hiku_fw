/*
 *  Copyright (C) 2008-2015 Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 * Assembly usage Application
 *
 * Summary:
 *
 * Adds 2 numbers using assembly functions and returns the result
 *
 * A serial terminal program like HyperTerminal, putty, or
 * minicom can be used to see the program output.
 */
#include <wmstdio.h>
#include <wm_os.h>


/*
 * The application entry point is main(). At this point, minimal
 * initialization of the hardware is done, and also the RTOS is
 * initialized.
 */

#define NUM1 123
#define NUM2 123

int asm_add_them(int x, int y);

int main(void)
{

	/* Initialize console on uart0 */
	wmstdio_init(UART0_ID, 0);

	wmprintf("Assembly usage demo Started\r\n");

	int result = 0;
	int x = NUM1;
	int y = NUM2;
	result = asm_add_them(x, y);
	wmprintf("Add %d and %d : \r\n", x, y);
	wmprintf("Result : %d\r\n", result);
	return 0;
}
