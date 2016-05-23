/*
 *  Copyright (C) 2008-2015 Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Description:   Example code demonstrating least square fit to data
 *                using matrix functions
 *
 *		  For more details, refer to arm_matrix_example_f32.c
 */
#include <wmstdio.h>
#include <wm_os.h>
#include <arm_math.h>
#include <mdev_fpu.h>


#define LOOP_CNT 1000
extern int32_t matrix_main(void);

int main(void)
{
	int i = 0;
	uint32_t ts;

	/* Initialize console on uart0 */
	wmstdio_init(UART0_ID, 0);

#ifdef __FPU_PRESENT
	fpu_init();
#endif

	wmprintf("DSP Matrix application Started\r\n");

	ts = os_get_timestamp();
	for (i = 0; i < LOOP_CNT; i++) {
		if (matrix_main() != ARM_MATH_SUCCESS) {
			wmprintf("Test failed: Iteration = %d\r\n", i + 1);
			break;
		}
	}
	ts = os_get_timestamp() - ts;

	wmprintf("Total Time = %u us for %d iterations\r\n", ts, i);
	return 0;
}
