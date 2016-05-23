/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wmstdio.h>
#include <wm_os.h>
#include <mdev_gpio.h>
#include <mdev_adc.h>
#include <mdev_pinmux.h>
#include <lowlevel_drivers.h>

/*
 * Simple Application which uses ADC driver.
 *
 * Summary:
 * This application uses ADC (analog to digital converter) to sample
 * an analog signal applied at GPIO_42 for MW300 and GPIO_7 for MC200.
 * This application by default samples 500 samples and prints the
 * output of ADC on console.
 *
 * Self calibration is also performed before conversion.
 *
 * DMA and well as non DMA mode is provided. Default is non-DMA.
 *
 * Description:
 *
 * This application is written using APIs exposed by MDEV
 * layer of ADC driver.
 *
 * The application configures the GPIO as Analog input to ADC0 peripheral.
 * ADC configurations used:
 * sampling frequency : 3.9KHz
 * Resolution: 16 bits
 * ADC gain: 2
 * Reference source for ADC: Internal VREF (1.2V)
 * Note: Maximum analog input voltage allowed with Internal VREF is 1.2V.
 * User may change the default ADC configuration as per the requirements.
 * When ADC gain is selected to 2; input voltage range reduces to
 * half (0.6V)
 */

/*------------------Macro Definitions ------------------*/
#define SAMPLES	500
#define ADC_GAIN	ADC_GAIN_2
#define BIT_RESOLUTION_FACTOR 32768	/* For 16 bit resolution (2^15-1) */
#define VMAX_IN_mV	1200	/* Max input voltage in milliVolts */
/* Default is IO mode, DMA mode can be enabled as per the requirement */
/*#define ADC_DMA*/


/*------------------Global Variable Definitions ---------*/

uint16_t buffer[SAMPLES];
mdev_t *adc_dev;

/* This is an entry point for the application.
   All application specific initialization is performed here. */
int main(void)
{
	int i, samples = SAMPLES;
	float result;

	wmstdio_init(UART0_ID, 0);

	wmprintf("\r\nADC Application Started\r\n");

	/* adc initialization */
	if (adc_drv_init(ADC0_ID) != WM_SUCCESS) {
		wmprintf("Error: Cannot init ADC\n\r");
		return -1;
	}

#if defined(CONFIG_CPU_MW300)
	adc_dev = adc_drv_open(ADC0_ID, ADC_CH0);

	i = adc_drv_selfcalib(adc_dev, vref_internal);
	if (i == WM_SUCCESS)
		wmprintf("Calibration successful!\r\n");
	else
		wmprintf("Calibration failed!\r\n");

	adc_drv_close(adc_dev);
#endif

	ADC_CFG_Type config;

	/* get default ADC gain value */
	adc_get_config(&config);
	wmprintf("Default ADC gain value = %d\r\n", config.adcGainSel);

	/* Modify ADC gain to 2 */
	adc_modify_default_config(adcGainSel, ADC_GAIN);

	adc_get_config(&config);
	wmprintf("Modified ADC gain value to %d\r\n", config.adcGainSel);

	adc_dev = adc_drv_open(ADC0_ID, ADC_CH0);

#ifdef ADC_DMA
	adc_drv_get_samples(adc_dev, buffer, samples);
	for (i = 0; i < samples; i++) {
		result = ((float)buffer[i] / BIT_RESOLUTION_FACTOR) *
		VMAX_IN_mV * ((float)1/(float)(config.adcGainSel != 0 ?
		config.adcGainSel : 0.5));
		wmprintf("Iteration %d: count %d - %d.%d mV\r\n",
					i, buffer[i],
					wm_int_part_of(result),
					wm_frac_part_of(result, 2));
	}
#else
	for (i = 0; i < samples; i++) {
		buffer[0] = adc_drv_result(adc_dev);
		result = ((float)buffer[0] / BIT_RESOLUTION_FACTOR) *
		VMAX_IN_mV * ((float)1/(float)(config.adcGainSel != 0 ?
		config.adcGainSel : 0.5));
		wmprintf("Iteration %d: count %d - %d.%d mV\r\n",
					i, buffer[0],
					wm_int_part_of(result),
					wm_frac_part_of(result, 2));
	}
#endif
	wmprintf("ADC Application Over\r\n");

	adc_drv_close(adc_dev);
	adc_drv_deinit(ADC0_ID);
	return 0;

}
