/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */
/* Summary:
 * This application demonstrates the use of DAC.
 * DAC is used to generate a traingular waveform.
 * DAC Channel B is used.
 * On MW300 the output can be observed at pin GPIO_43
 * and on MC200 on GPIO_11.
 * Connect a DSO to this pin and you can see the waveform.
*/
#include <wmstdio.h>
#include <wm_os.h>
#include <mdev.h>
#include <mdev_dac.h>
#include <mdev_pinmux.h>
#include <lowlevel_drivers.h>

/* DAC is selected in 10 bit resolution */
#define MAX_COUNT 1023

/*
 * The application entry point is main(). At this point, minimal
 * initialization of the hardware is done, and also the RTOS is
 * initialized.
 */
int main(void)
{
	/* Initialize console on uart0 */
	wmstdio_init(UART0_ID, 0);
	wmprintf("DAC Application started\r\n");
	/* Initialise DAC */
	dac_drv_init();
	/* Change Congiguration to normal wave */
	dac_modify_default_config(waveMode, DAC_WAVE_NORMAL);
	/* Change Congiguration to DAC ouput pad  */
	dac_modify_default_config(outMode, DAC_OUTPUT_PAD);
	/* Change Congiguration to  DAC timing correlated*/
	dac_modify_default_config(timingMode, DAC_TIMING_CORRELATED);
	mdev_t *dac_dev;
	/* Open the dac driver with given configuration */
	dac_dev = dac_drv_open(MDEV_DAC, DAC_CH_B);
	if (dac_dev == NULL) {
		wmprintf("DAC driver open failed\r\n");
		return -WM_FAIL;
	}
	int i;
	/* Traingular Waveform */
	while (1) {
		for (i = 0; i < MAX_COUNT; i++) {
			dac_drv_output(dac_dev, i);
			os_thread_sleep(1);
		}
		for (i = MAX_COUNT; i >= 0; i--) {
			dac_drv_output(dac_dev, i);
			os_thread_sleep(1);
		}
	}
return 0;
}
