/*
 *  Copyright (C) 2008-2015 Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Description:   Example code demonstrating floating point
 * 		  operations
 *
 *		  For more details, refer to arm_dotproduct_example_f32.c
 */
#include <wmstdio.h>
#include <wm_os.h>
#include <arm_math.h>
#include <mdev_fpu.h>


int32_t dotproduct_main(void);

int main(void)
{
	int i = 0;
	uint32_t ts;

	/* Initialize console on uart0 */
	wmstdio_init(UART0_ID, 0);

#ifdef __FPU_PRESENT
	fpu_init();
#endif

	wmprintf("DSP Dot Product application Started\r\n");

	ts = os_get_timestamp();
	if (dotproduct_main() != ARM_MATH_SUCCESS) {
		wmprintf("Test failed\r\n");
		return 0;
	}
	ts = os_get_timestamp() - ts;

	wmprintf("Total Time = %u us\r\n", ts, i);
	return 0;
}
