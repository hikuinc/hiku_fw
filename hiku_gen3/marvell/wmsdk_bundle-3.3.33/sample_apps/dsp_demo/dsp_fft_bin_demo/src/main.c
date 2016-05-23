/*
 *  Copyright (C) 2008-2015 Marvell International Ltd.
 *  All Rights Reserved.
 */

/*
 * Description:   Example code demonstrating calculation of Max energy bin of
 *                frequency domain of input signal.
 *
 *		  For more details, refer to arm_fft_bin_example_f32.c
 */
#include <wmstdio.h>
#include <wm_os.h>
#include <arm_math.h>
#include <mdev_fpu.h>


#define LOOP_CNT 1
extern int32_t fft_bin_main(void);

int main(void)
{
	int i = 0;
	uint32_t ts;

	/* Initialize console on uart0 */
	wmstdio_init(UART0_ID, 0);

#ifdef __FPU_PRESENT
	fpu_init();
#endif

	wmprintf("DSP FFT application Started\r\n");

	ts = os_get_timestamp();
	for (i = 0; i < LOOP_CNT; i++) {
		if (fft_bin_main() != ARM_MATH_SUCCESS) {
			wmprintf("Test failed: Iteration = %d\r\n", i + 1);
			break;
		}
	}
	ts = os_get_timestamp() - ts;

	wmprintf("Total Time = %u us for %d iterations\r\n", ts, i);
	return 0;
}
