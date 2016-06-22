#include "asf.h"
#include "conf_board.h"
#include "conf_clock.h"

// All OpenCV headers (excluding internal ones prefixed with an underscore)
#include "cvaux.h"
#include "cxmisc.h"
#include "cvvidsurv.hpp"
#include "cv.hpp"
#include "cvmat.hpp"
#include "cxtypes.h"
#include "cvtypes.h"
#include "cxcore.h"
#include "cv.h"
#include "cxcore.hpp"
#include "cxerror.h"
#include "cvaux.hpp"


#include "zbar.h"


uint8_t teststring[1140] = {0xd9,0xdc,0xdc,0xd9,0xdb,0xdd,0xda,0xd8,0xdd,0xda,0xd8,0xdc,0xd9,0xd6,0xda,0xda,0xd8,0xdb,0xd7,0xd6,0xda,0xd9,0xd8,0xd8,0xd8,0xd9,0xdb,0xd6,0xd7,0xd9,0xd8,0xdb,0xdb,0xd4,0xd7,0xd8,0xd7,0xd9,0xd9,0xd5,0xda,0xd7,0xd8,0xd5,0xd6,0xd8,0xdc,0xd6,0xd6,0xd6,0xd6,0xd9,0xdb,0xd5,0xd5,0xd7,0xd7,0xda,0xd9,0xd5,0xd4,0xd6,0xda,0xdc,0xd9,0xd6,0xd7,0xd5,0xd8,0xda,0xda,0xd8,0xd7,0xd5,0xd7,0xd8,0xda,0xdb,0xd6,0xd7,0xd9,0xd6,0xd9,0xd9,0xd5,0xd9,0xdb,0xd6,0xd7,0xd3,0xd5,0xd8,0xda,0xd4,0xd6,0xd6,0xd4,0xd5,0xd5,0xd4,0xd2,0xd7,0xd6,0xd6,0xd5,0xd5,0xd2,0xd3,0xa6,0xa2,0xa1,0x9a,0x95,0x93,0x46,0x46,0x47,0x48,0x41,0x43,0xa2,0xaa,0xaf,0xb3,0xab,0xac,0xcf,0xcc,0xcb,0xcd,0xcb,0xcc,0x6a,0x64,0x67,0x68,0x62,0x5f,0x87,0x8d,0x97,0x94,0x91,0x92,0xd4,0xd7,0xd8,0xd7,0xd7,0xd6,0xcd,0xd0,0xd1,0xd1,0xd0,0xd1,0xd2,0xd1,0xd1,0xcf,0xcf,0xcf,0x83,0x7f,0x7e,0x76,0x72,0x6f,0x5e,0x5e,0x60,0x62,0x6c,0x6e,0xc8,0xcb,0xca,0xcb,0xd1,0xcf,0xcf,0xd4,0xd3,0xd1,0xd0,0xd1,0xcf,0xcf,0xd3,0xd2,0xd3,0xd2,0xd1,0xce,0xd0,0xd2,0xd1,0xd5,0xcc,0xc5,0xc9,0xc8,0xc5,0xc8,0x70,0x67,0x6b,0x65,0x61,0x62,0x81,0x85,0x87,0x90,0x91,0x91,0xd2,0xd2,0xd2,0xd1,0xd2,0xd1,0x8e,0x88,0x81,0x7c,0x79,0x73,0x4d,0x47,0x49,0x4a,0x49,0x51,0xb4,0xb8,0xb9,0xb8,0xb8,0xc2,0xd0,0xd0,0xce,0xc7,0xcb,0xd7,0xcd,0xcb,0xcd,0xc8,0xcd,0xd6,0x9c,0x97,0x94,0x91,0x91,0x8c,0x32,0x2e,0x2e,0x30,0x2a,0x29,0x23,0x22,0x24,0x27,0x25,0x26,0x27,0x25,0x29,0x28,0x25,0x24,0x20,0x22,0x26,0x26,0x20,0x20,0x6c,0x6b,0x70,0x76,0x7e,0x7c,0xca,0xc5,0xcc,0xca,0xcb,0xcb,0x93,0x8e,0x93,0x8a,0x85,0x82,0x43,0x43,0x48,0x48,0x4b,0x4e,0xa6,0xac,0xaf,0xb3,0xb6,0xb8,0xce,0xcc,0xcc,0xc9,0xca,0xcc,0xcc,0xc9,0xca,0xc7,0xca,0xce,0x9d,0x9e,0x9d,0x95,0x91,0x93,0x31,0x33,0x37,0x2f,0x2f,0x30,0x20,0x27,0x23,0x23,0x27,0x23,0x27,0x28,0x24,0x28,0x24,0x22,0x1f,0x22,0x1f,0x20,0x1e,0x20,0x50,0x56,0x5a,0x5e,0x62,0x6c,0xbe,0xc1,0xc3,0xc6,0xc8,0xc8,0xc9,0xcd,0xcc,0xc9,0xca,0xc8,0xcb,0xce,0xcc,0xcc,0xcf,0xcd,0xa6,0xa0,0x9d,0x9c,0x95,0x92,0x57,0x56,0x56,0x58,0x58,0x5b,0xb1,0xb3,0xb7,0xb8,0xbb,0xc1,0xd0,0xcd,0xcc,0xcd,0xcc,0xce,0xcd,0xcb,0xcc,0xcc,0xc9,0xcd,0xce,0xcc,0xcd,0xc9,0xc9,0xcd,0xcf,0xcf,0xcf,0xcc,0xcd,0xcd,0xac,0xa7,0xa2,0x9f,0x9f,0x94,0x53,0x55,0x54,0x56,0x5a,0x59,0xa7,0xae,0xb1,0xb3,0xb1,0xbc,0xb9,0xb1,0xb0,0xab,0xa6,0xa4,0x4a,0x47,0x4b,0x47,0x47,0x48,0x7b,0x84,0x86,0x8a,0x92,0x9c,0xc9,0xce,0xc9,0xc9,0xc9,0xcc,0xc1,0xc6,0xc6,0xc6,0xc5,0xc9,0xc1,0xc1,0xbe,0xbb,0xb8,0xb6,0x6d,0x67,0x5d,0x58,0x57,0x4d,0x1d,0x1c,0x1c,0x1c,0x1d,0x1d,0x1c,0x1f,0x24,0x21,0x1f,0x22,0x1a,0x1a,0x1b,0x1b,0x1b,0x1b,0x26,0x26,0x2a,0x2d,0x2f,0x37,0x90,0x94,0x9a,0x9e,0x9e,0xa9,0xbe,0xbd,0xbb,0xbb,0xb9,0xb3,0x63,0x5a,0x57,0x52,0x4f,0x4a,0x5f,0x60,0x69,0x73,0x78,0x81,0xc2,0xc2,0xc6,0xc7,0xc7,0xc9,0xc4,0xc3,0xc2,0xbf,0xc4,0xc5,0xbe,0xbe,0xbf,0xbb,0xbb,0xb9,0x74,0x6e,0x6d,0x62,0x5d,0x5c,0x1f,0x21,0x20,0x1c,0x1f,0x20,0x1e,0x1d,0x1e,0x1e,0x20,0x21,0x1d,0x1d,0x1d,0x1e,0x1d,0x1b,0x1c,0x20,0x22,0x25,0x23,0x29,0x78,0x7f,0x89,0x8c,0x89,0x98,0xbb,0xbc,0xbe,0xbe,0xbb,0xbb,0x6b,0x6a,0x63,0x61,0x5a,0x4f,0x3e,0x3f,0x46,0x4d,0x54,0x59,0xaf,0xb3,0xb8,0xbc,0xc1,0xc1,0xb3,0xae,0xa6,0xa1,0x9e,0x96,0x53,0x4d,0x49,0x4c,0x51,0x4e,0x8b,0x91,0x9d,0xa6,0xaf,0xaf,0xc0,0xc0,0xbe,0xbc,0xba,0xb8,0x6f,0x65,0x5a,0x56,0x52,0x4d,0x44,0x48,0x4c,0x53,0x5c,0x64,0xaf,0xb3,0xb6,0xba,0xbe,0xbb,0x9a,0x98,0x8e,0x89,0x84,0x75,0x31,0x31,0x2f,0x2c,0x2a,0x26,0x18,0x1c,0x1b,0x18,0x18,0x1c,0x2d,0x33,0x37,0x40,0x47,0x4f,0x9d,0xa6,0xa9,0xaf,0xb6,0xba,0xc2,0xc3,0xc3,0xc4,0xc5,0xc7,0xc0,0xc4,0xc6,0xc4,0xc2,0xc5,0xc3,0xc6,0xc5,0xc3,0xc2,0xc3,0xc4,0xc4,0xc5,0xc2,0xc1,0xbd,0x78,0x74,0x6f,0x63,0x5d,0x57,0x3d,0x3e,0x44,0x4f,0x52,0x59,0xab,0xad,0xb1,0xbc,0xbb,0xbd,0xaa,0xa1,0x97,0x97,0x91,0x87,0x43,0x39,0x36,0x34,0x31,0x2e,0x1d,0x1c,0x1b,0x19,0x17,0x1b,0x2b,0x30,0x39,0x41,0x46,0x4c,0x9e,0xa1,0xac,0xb3,0xb2,0xb7,0xc5,0xc2,0xc3,0xc7,0xc3,0xc5,0xc5,0xc5,0xc5,0xc6,0xc4,0xc6,0xc6,0xc6,0xc6,0xc6,0xc1,0xc4,0xcb,0xca,0xc9,0xc8,0xbe,0xc2,0x85,0x7e,0x78,0x72,0x65,0x5f,0x55,0x56,0x62,0x67,0x70,0x7c,0xba,0xbe,0xc4,0xc1,0xc3,0xc9,0xbf,0xc6,0xc6,0xc4,0xc4,0xc4,0xc0,0xc6,0xc8,0xc4,0xc3,0xc4,0xc2,0xc6,0xc7,0xc6,0xc5,0xc6,0xc3,0xc4,0xbf,0xba,0xb8,0xb5,0x73,0x6a,0x65,0x5b,0x54,0x4e,0x28,0x23,0x22,0x21,0x21,0x24,0x1d,0x1f,0x1f,0x24,0x2a,0x32,0x74,0x81,0x8a,0x94,0x9c,0xa2,0xc3,0xc4,0xc1,0xbd,0xb7,0xb8,0x75,0x6b,0x5f,0x5f,0x55,0x4f,0x5f,0x62,0x6b,0x7e,0x81,0x89,0xc1,0xc0,0xc2,0xc8,0xc3,0xc9,0xc0,0xbf,0xc2,0xbf,0xc0,0xc5,0xc1,0xbf,0xbe,0xb9,0xb5,0xb5,0x72,0x68,0x5e,0x52,0x4d,0x49,0x23,0x23,0x24,0x24,0x25,0x22,0x1d,0x23,0x25,0x2d,0x2f,0x2e,0x7f,0x8a,0x8d,0x9b,0xa2,0xa7,0xc4,0xc0,0xc4,0xc6,0xc3,0xc2,0xc1,0xc0,0xc3,0xc2,0xc2,0xc8,0xc2,0xbd,0xb7,0xad,0xa3,0x9f,0x60,0x57,0x50,0x43,0x3d,0x36,0x24,0x21,0x22,0x1f,0x1f,0x1e,0x2f,0x33,0x3f,0x47,0x50,0x5b,0x9d,0xa8,0xb4,0xb7,0xbb,0xbf,0xc3,0xc1,0xc6,0xc3,0xc3,0xc4,0xc3,0xbf,0xc3,0xc2,0xc1,0xc6,0xc5,0xc3,0xc6,0xc2,0xc2,0xca,0xc7,0xc1,0xbe,0xb1,0xa8,0xa4,0x5f,0x56,0x49,0x45,0x43,0x39,0x6b,0x72,0x72,0x82,0x93,0x99,0xc4,0xc9,0xc3,0xbc,0xba,0xba,0x72,0x6c,0x5f,0x5a,0x4f,0x4a,0x51,0x59,0x5f,0x71,0x7b,0x82,0xc3,0xc6,0xc1,0xbf,0xc9,0xc0,0x9e,0x90,0x7a,0x72,0x6f,0x63,0x3b,0x33,0x29,0x2b,0x29,0x2b,0x28,0x25,0x24,0x24,0x25,0x2b,0x5f,0x6d,0x7c,0x8a,0x96,0x9c,0xc4,0xc6,0xc6,0xc7,0xc8,0xc7,0xc6,0xc6,0xc3,0xc2,0xc2,0xc1,0xc5,0xc6,0xc2,0xc2,0xc4,0xc4,0xcd,0xcb,0xc8,0xc9,0xc3,0xc2,0x98,0x86,0x74,0x72,0x61,0x5a,0x44,0x44,0x41,0x4c,0x55,0x6b,0xab,0xb7,0xbc,0xc0,0xc5,0xcd,0xae,0xac,0xa6,0x92,0x83,0x7f,0x48,0x4c,0x51,0x56,0x5c,0x61,0xa2,0xab,0xb4,0xbe,0xc1,0xc2,0xcb,0xc9,0xc5,0xc4,0xc5,0xc6,0xc7,0xc7,0xc0,0xc2,0xc7,0xc8,0xc8,0xca,0xc8,0xc5,0xc8,0xc6,0xc9,0xcb,0xca,0xc6,0xc9,0xc7,0xcc,0xcc,0xc6,0xc9,0xc9,0xc9,0xcb,0xcd,0xc5,0xca,0xc7,0xc9,0xca,0xcc,0xc7,0xcb,0xc6,0xc8,0xc7,0xca,0xcd,0xc9,0xc6,0xc9,0xc4,0xcc,0xcd,0xc6,0xc6,0xce,};


/* Uncomment this macro to work in black and white mode */
//#define DEFAULT_MODE_COLORED

#ifndef PIO_PCMR_DSIZE_WORD
#  define PIO_PCMR_DSIZE_WORD PIO_PCMR_DSIZE(2)
#endif

/* TWI clock frequency in Hz (400KHz) */
#define TWI_CLK     (400000UL)

/* Pointer to the image data destination buffer */
uint8_t *g_p_uc_cap_dest_buf;

/* Rows size of capturing picture */
uint16_t g_us_cap_rows = IMAGE_HEIGHT;

/* Define display function and line size of captured picture according to the */
/* current mode (color or black and white) */
#ifdef DEFAULT_MODE_COLORED
	#define _display() draw_frame_yuv_color_int()

	/* (IMAGE_WIDTH *2 ) because ov7740 use YUV422 format in color mode */
	/* (draw_frame_yuv_color_int for more details) */
	uint16_t g_us_cap_line = (IMAGE_WIDTH * 2);
#else
	#define _display() draw_frame_yuv_bw8()

	uint16_t g_us_cap_line = (IMAGE_WIDTH);
#endif

/* Push button information (true if it's triggered and false otherwise) */
static volatile uint32_t g_ul_push_button_trigger = false;

/* Vsync signal information (true if it's triggered and false otherwise) */
static volatile uint32_t g_ul_vsync_flag = false;

static volatile uint32_t g_display_splash = false;
/**
 * \brief Handler for vertical synchronisation using by the OV7740 image
 * sensor.
 */
static void vsync_handler(uint32_t ul_id, uint32_t ul_mask)
{
	unused(ul_id);
	unused(ul_mask);

	g_ul_vsync_flag = true;
}

/**
 * \brief Handler for button rising edge interrupt.
 */
static void button_handler(uint32_t ul_id, uint32_t ul_mask)
{
	unused(ul_id);
	unused(ul_mask);

	if (g_ul_push_button_trigger){
		g_ul_push_button_trigger = false;
		g_display_splash = true;

	} else {
		g_ul_push_button_trigger = true;
	}


}

/**
 * \brief Intialize Vsync_Handler.
 */
static void init_vsync_interrupts(void)
{
	/* Initialize PIO interrupt handler, see PIO definition in conf_board.h
	**/
	pio_handler_set(OV7740_VSYNC_PIO, OV7740_VSYNC_ID, OV7740_VSYNC_MASK,
			OV7740_VSYNC_TYPE, vsync_handler);

	/* Enable PIO controller IRQs */
	NVIC_EnableIRQ((IRQn_Type)OV7740_VSYNC_ID);
}

/**
 * \brief Configure push button and initialize button_handler interrupt.
 */
static void configure_button(void)
{
	/* Configure PIO clock. */
	pmc_enable_periph_clk(PUSH_BUTTON_ID);

	/* Adjust PIO debounce filter using a 10 Hz filter. */
	pio_set_debounce_filter(PUSH_BUTTON_PIO, PUSH_BUTTON_PIN_MSK, 10);

	/* Initialize PIO interrupt handler, see PIO definition in conf_board.h
	**/
	pio_handler_set(PUSH_BUTTON_PIO, PUSH_BUTTON_ID, PUSH_BUTTON_PIN_MSK,
			PUSH_BUTTON_ATTR, button_handler);

	/* Enable PIO controller IRQs. */
	NVIC_EnableIRQ((IRQn_Type)PUSH_BUTTON_ID);

	/* Enable PIO interrupt lines. */
	pio_enable_interrupt(PUSH_BUTTON_PIO, PUSH_BUTTON_PIN_MSK);
}

/**
 * \brief Initialize PIO capture for the OV7740 image sensor communication.
 *
 * \param p_pio PIO instance to be configured in PIO capture mode.
 * \param ul_id Corresponding PIO ID.
 */
static void pio_capture_init(Pio *p_pio, uint32_t ul_id)
{
	/* Enable periphral clock */
	pmc_enable_periph_clk(ul_id);

	/* Disable pio capture */
	p_pio->PIO_PCMR &= ~((uint32_t)PIO_PCMR_PCEN);

	/* Disable rxbuff interrupt */
	p_pio->PIO_PCIDR |= PIO_PCIDR_RXBUFF;

	/* 32bit width*/
	p_pio->PIO_PCMR &= ~((uint32_t)PIO_PCMR_DSIZE_Msk);
	p_pio->PIO_PCMR |= PIO_PCMR_DSIZE_WORD;

	/* Only HSYNC and VSYNC enabled */
	p_pio->PIO_PCMR &= ~((uint32_t)PIO_PCMR_ALWYS);
	p_pio->PIO_PCMR &= ~((uint32_t)PIO_PCMR_HALFS);

#if !defined(DEFAULT_MODE_COLORED)
	/* Samples only data with even index */
	p_pio->PIO_PCMR |= PIO_PCMR_HALFS;
	p_pio->PIO_PCMR &= ~((uint32_t)PIO_PCMR_FRSTS);
#endif
}

/**
 * \brief Capture OV7740 data to a buffer.
 *
 * \param p_pio PIO instance which will capture data from OV7740 iamge sensor.
 * \param p_uc_buf Buffer address where captured data must be stored.
 * \param ul_size Data frame size.
 */
static uint8_t pio_capture_to_buffer(Pio *p_pio, uint8_t *uc_buf,
		uint32_t ul_size)
{
	/* Check if the first PDC bank is free */
	if ((p_pio->PIO_RCR == 0) && (p_pio->PIO_RNCR == 0)) {
		p_pio->PIO_RPR = (uint32_t)uc_buf;
		p_pio->PIO_RCR = ul_size;
		p_pio->PIO_PTCR = PIO_PTCR_RXTEN;
		return 1;
	} else if (p_pio->PIO_RNCR == 0) {
		p_pio->PIO_RNPR = (uint32_t)uc_buf;
		p_pio->PIO_RNCR = ul_size;
		return 1;
	} else {
		return 0;
	}
}

/**
 * \brief Intialize LCD display.
 */
static void display_init(void)
{
	struct ili9325_opt_t ili9325_display_opt;

	/* Enable peripheral clock */
	pmc_enable_periph_clk( ID_SMC );

	/* Configure SMC interface for LCD */
	smc_set_setup_timing(SMC, ILI9325_LCD_CS, SMC_SETUP_NWE_SETUP(2)
			| SMC_SETUP_NCS_WR_SETUP(2)
			| SMC_SETUP_NRD_SETUP(2)
			| SMC_SETUP_NCS_RD_SETUP(2));

	smc_set_pulse_timing(SMC, ILI9325_LCD_CS, SMC_PULSE_NWE_PULSE(4)
			| SMC_PULSE_NCS_WR_PULSE(4)
			| SMC_PULSE_NRD_PULSE(10)
			| SMC_PULSE_NCS_RD_PULSE(10));

	smc_set_cycle_timing(SMC, ILI9325_LCD_CS, SMC_CYCLE_NWE_CYCLE(10)
			| SMC_CYCLE_NRD_CYCLE(22));

	smc_set_mode(SMC, ILI9325_LCD_CS, SMC_MODE_READ_MODE
			| SMC_MODE_WRITE_MODE);

	/* Initialize display parameter */
	ili9325_display_opt.ul_width = ILI9325_LCD_WIDTH;
	ili9325_display_opt.ul_height = ILI9325_LCD_HEIGHT;
	ili9325_display_opt.foreground_color = COLOR_BLACK;
	ili9325_display_opt.background_color = COLOR_WHITE;

	/* Switch off backlight */
	aat31xx_disable_backlight();

	/* Initialize LCD */
	ili9325_init(&ili9325_display_opt);

	/* Set backlight level */
	aat31xx_set_backlight(AAT31XX_MAX_BACKLIGHT_LEVEL);

	/* Turn on LCD */
	ili9325_display_on();
}

/**
 * \brief Initialize PIO capture and the OV7740 image sensor.
 */
static void capture_init(void)
{
	twi_options_t opt;

	/* Init Vsync handler*/
	init_vsync_interrupts();

	/* Init PIO capture*/
	pio_capture_init(OV_DATA_BUS_PIO, OV_DATA_BUS_ID);

	/* Turn on ov7740 image sensor using power pin */
	ov_power(true, OV_POWER_PIO, OV_POWER_MASK);

	/* Init PCK0 to work at 24 Mhz */
	/* 96/4=24 Mhz */
	PMC->PMC_PCK[0] = (PMC_PCK_PRES_CLK_4 | PMC_PCK_CSS_PLLA_CLK);
	PMC->PMC_SCER = PMC_SCER_PCK0;
	while (!(PMC->PMC_SCSR & PMC_SCSR_PCK0)) {
	}

	/* Enable TWI peripheral */
	pmc_enable_periph_clk(ID_BOARD_TWI);

	/* Init TWI peripheral */
	opt.master_clk = sysclk_get_cpu_hz();
	opt.speed      = TWI_CLK;
	twi_master_init(BOARD_TWI, &opt);

	/* Configure TWI interrupts */
	NVIC_DisableIRQ(BOARD_TWI_IRQn);
	NVIC_ClearPendingIRQ(BOARD_TWI_IRQn);
	NVIC_SetPriority(BOARD_TWI_IRQn, 0);
	NVIC_EnableIRQ(BOARD_TWI_IRQn);

	/* ov7740 Initialization */
	while (ov_init(BOARD_TWI) == 1) {
	}

	/* ov7740 configuration */
	ov_configure(BOARD_TWI, QVGA_YUV422_20FPS);

	/* Wait 3 seconds to let the image sensor to adapt to environment */
	delay_ms(3000);
}

/**
 * \brief Start picture capture.
 */
static void start_capture(void)
{
	/* Set capturing destination address*/
	g_p_uc_cap_dest_buf = (uint8_t *)CAP_DEST;

	/* Set cap_rows value*/
	g_us_cap_rows = IMAGE_HEIGHT;

	/* Enable vsync interrupt*/
	pio_enable_interrupt(OV7740_VSYNC_PIO, OV7740_VSYNC_MASK);

	/* Capture acquisition will start on rising edge of Vsync signal.
	 * So wait g_vsync_flag = 1 before start process
	 */
	while (!g_ul_vsync_flag) {
	}

	/* Disable vsync interrupt*/
	pio_disable_interrupt(OV7740_VSYNC_PIO, OV7740_VSYNC_MASK);

	/* Enable pio capture*/
	pio_capture_enable(OV7740_DATA_BUS_PIO);

	/* Capture data and send it to external SRAM memory thanks to PDC
	 * feature */
	pio_capture_to_buffer(OV7740_DATA_BUS_PIO, g_p_uc_cap_dest_buf,
			(g_us_cap_line * g_us_cap_rows) >> 2);

	/* Wait end of capture*/
	while (!((OV7740_DATA_BUS_PIO->PIO_PCISR & PIO_PCIMR_RXBUFF) ==
			PIO_PCIMR_RXBUFF)) {
	}

	/* Disable pio capture*/
	pio_capture_disable(OV7740_DATA_BUS_PIO);

	/* Reset vsync flag*/
	g_ul_vsync_flag = false;
}

/**
 * \brief Configure SMC interface for SRAM.
 */
static void board_configure_sram( void )
{
	/* Enable peripheral clock */
	pmc_enable_periph_clk( ID_SMC );

	/* Configure SMC interface for SRAM */
	smc_set_setup_timing(SMC, SRAM_CS, SMC_SETUP_NWE_SETUP(2)
			| SMC_SETUP_NCS_WR_SETUP(0)
			| SMC_SETUP_NRD_SETUP(3)
			| SMC_SETUP_NCS_RD_SETUP(0));

	smc_set_pulse_timing(SMC, SRAM_CS, SMC_PULSE_NWE_PULSE(4)
			| SMC_PULSE_NCS_WR_PULSE(5)
			| SMC_PULSE_NRD_PULSE(4)
			| SMC_PULSE_NCS_RD_PULSE(6));

	smc_set_cycle_timing(SMC, SRAM_CS, SMC_CYCLE_NWE_CYCLE(6)
			| SMC_CYCLE_NRD_CYCLE(7));

	smc_set_mode(SMC, SRAM_CS, SMC_MODE_READ_MODE
			| SMC_MODE_WRITE_MODE);
}

#ifdef DEFAULT_MODE_COLORED

/**
 * \brief Take a 32 bit variable in parameters and returns a value between 0 and
 * 255.
 *
 * \param i Enter value .
 * \return i if 0<i<255, 0 if i<0 and 255 if i>255
 */
static inline uint8_t clip32_to_8( int32_t i )
{
	if (i > 255) {
		return 255;
	}

	if (i < 0) {
		return 0;
	}

	return (uint8_t)i;
}

static void draw_frame_yuv_color_int( void )
{
	uint32_t ul_cursor;
	int32_t l_y1;
	int32_t l_y2;
	int32_t l_v;
	int32_t l_u;
	int32_t l_blue;
	int32_t l_green;
	int32_t l_red;
	uint8_t *p_uc_data;
	
	volatile uint8_t *p_y_data;
	volatile uint32_t tempcursor;
	
	p_uc_data = (uint8_t *)g_p_uc_cap_dest_buf;
	p_y_data = (uint8_t *)g_p_uc_cap_dest_buf;
	
	/* Configure LCD to draw captured picture */
	LCD_IR(0);
	LCD_IR(ILI9325_ENTRY_MODE);
	LCD_WD(((ILI9325_ENTRY_MODE_BGR | ILI9325_ENTRY_MODE_AM |
			ILI9325_ENTRY_MODE_DFM | ILI9325_ENTRY_MODE_TRI |
			ILI9325_ENTRY_MODE_ORG) >> 8) & 0xFF);
	LCD_WD((ILI9325_ENTRY_MODE_BGR | ILI9325_ENTRY_MODE_AM |
			ILI9325_ENTRY_MODE_DFM | ILI9325_ENTRY_MODE_TRI |
			ILI9325_ENTRY_MODE_ORG) & 0xFF);
	ili9325_draw_prepare(0, 0, IMAGE_HEIGHT, IMAGE_WIDTH);

	/* OV7740 Color format is YUV422. In this format pixel has 4 bytes
	 * length (Y1,U,Y2,V).
	 * To display it on LCD,these pixel need to be converted in RGB format.
	 * The output of this conversion is two 3 bytes pixels in (B,G,R)
	 * format. First one is calculed using Y1,U,V and the other one with
	 * Y2,U,V. For that reason cap_line is twice bigger in color mode
	 * than in black and white mode. */
	for (ul_cursor = IMAGE_WIDTH * IMAGE_HEIGHT; ul_cursor != 0;
			ul_cursor -= 2, p_uc_data += 4, p_y_data += 2) {
		l_y1 = p_uc_data[0]; /* Y1 */
		l_y1 -= 16;
		l_v = p_uc_data[3]; /* V */
		l_v -= 128;
		l_u = p_uc_data[1]; /* U */
		l_u -= 128;

		l_blue = 516 * l_v + 128;
		l_green = -100 * l_v - 208 * l_u + 128;
		l_red = 409 * l_u + 128;

		/* BLUE */
		LCD_WD( clip32_to_8((298 * l_y1 + l_blue) >> 8));
		/* GREEN */
		LCD_WD( clip32_to_8((298 * l_y1 + l_green) >> 8));
		/* RED */
		LCD_WD( clip32_to_8((298 * l_y1 + l_red) >> 8));

		l_y2 = p_uc_data[2]; /* Y2 */
		l_y2 -= 16;
		LCD_WD( clip32_to_8((298 * l_y2 + l_blue) >> 8));
		LCD_WD( clip32_to_8((298 * l_y2 + l_green) >> 8));
		LCD_WD( clip32_to_8((298 * l_y2 + l_red) >> 8));				
		
		p_y_data[0] = p_uc_data[0];		//	Y1
		p_y_data[1] = p_uc_data[2];		//	Y2
		
		tempcursor = ul_cursor;
	}
}

#else

/**
 * \brief Draw LCD in black and white with integral algorithm.
 */
static void draw_frame_yuv_bw8( void )
{
	volatile uint32_t ul_cursor;
	uint8_t *p_uc_data;

	p_uc_data = (uint8_t *)g_p_uc_cap_dest_buf;

	/* Configure LCD to draw captured picture */
	LCD_IR(0);
	LCD_IR(ILI9325_ENTRY_MODE);
	LCD_WD(((ILI9325_ENTRY_MODE_BGR | ILI9325_ENTRY_MODE_AM |
			ILI9325_ENTRY_MODE_DFM | ILI9325_ENTRY_MODE_TRI |
			ILI9325_ENTRY_MODE_ORG) >> 8) & 0xFF);
	LCD_WD((ILI9325_ENTRY_MODE_BGR | ILI9325_ENTRY_MODE_AM |
			ILI9325_ENTRY_MODE_DFM | ILI9325_ENTRY_MODE_TRI |
			ILI9325_ENTRY_MODE_ORG) & 0xFF);
	ili9325_draw_prepare(0, 0, IMAGE_HEIGHT, IMAGE_WIDTH);

	/* LCD pixel has 24bit data. In black and White mode data has 8bit only
	 * so
	 * this data must be written three time in LCD memory.
	 */
	for (ul_cursor = IMAGE_WIDTH * IMAGE_HEIGHT; ul_cursor != 0;
			ul_cursor--, p_uc_data++) {
		/* Black and White using Y */
		LCD_WD( *p_uc_data );
		LCD_WD( *p_uc_data );
		LCD_WD( *p_uc_data );
	}
}

#endif

uint32_t g_ul_begin_capture_time = 0;
uint32_t g_ul_end_capture_time = 0;
uint32_t g_ul_begin_process_time = 0;
uint32_t g_ul_end_process_time = 0;
uint32_t g_ul_elapsed_time = 0;

volatile uint32_t g_ms_ticks = 0;

void SysTick_Handler(void)
{
    g_ms_ticks++;
}

void time_tick_init(void)
{
    g_ms_ticks = 0;
    SysTick_Config(sysclk_get_cpu_hz() / 1000); 
}

uint32_t time_tick_get(void)
{
    return g_ms_ticks;
}


extern "C" void zbar_hiku_process(void);

/*void zbar_process(void){

			zbar_image_scanner_t *scanner = NULL;
			scanner = zbar_image_scanner_create();
			zbar_image_scanner_set_config(scanner, ZBAR_NONE, ZBAR_CFG_ENABLE, 1);
	
			uint8_t * temp = (uint8_t *)CAP_DEST;
			zbar_image_t *image = zbar_image_create();
			zbar_image_set_format(image, zbar_fourcc('G','R','E','Y'));
			zbar_image_set_size(image, IMAGE_WIDTH, IMAGE_HEIGHT);
			zbar_image_set_data(image, temp, IMAGE_WIDTH * IMAGE_HEIGHT, zbar_image_free_data);

			int n = zbar_scan_image(scanner, image);

			const zbar_symbol_t *symbol = zbar_image_first_symbol(image);
			for(; symbol; symbol = zbar_symbol_next(symbol)) {
				
				g_ul_end_process_time = time_tick_get();

				char capture_time[32];
				char process_time[32];
				char total_time[32];
				
				sprintf(capture_time, "%u ms", g_ul_end_capture_time - g_ul_begin_capture_time);
				sprintf(process_time, "%u ms", g_ul_end_process_time - g_ul_begin_process_time);
				sprintf(total_time, "%u ms", time_tick_get() - g_ul_begin_capture_time);

				// print the results 
				zbar_symbol_type_t typ = zbar_symbol_get_type(symbol);
				volatile const char *data = zbar_symbol_get_data(symbol);
	
				//printf("decoded %s symbol \"%s\"\n", zbar_get_symbol_name(typ), data);
				display_init();
				ili9325_fill(COLOR_BLUE);
				ili9325_draw_string(0, 20, (uint8_t *)data);
				ili9325_draw_string(0, 80, (uint8_t *)zbar_get_symbol_name(typ));
				ili9325_draw_string(0, 100, (uint8_t *)capture_time );
				ili9325_draw_string(0, 120, (uint8_t *)process_time );
				ili9325_draw_string(0, 140, (uint8_t *)total_time );

				g_ul_push_button_trigger = false;


			}
			
}*/









/**
 * \brief Application entry point for image sensor capture example.
 *
 * \return Unused (ANSI-C compatibility).
 */
int main(void)
{
	sysclk_init();
	board_init();

	/* OV7740 send image sensor data at 24 Mhz. For best performances, PCK0
	 * which will capture OV7740 data, has to work at 24Mhz. It's easier and
	 * optimum to use one PLL for core (PLLB) and one other for PCK0 (PLLA).
	 */
	pmc_enable_pllack(7, 0x1, 1); /* PLLA work at 96 Mhz */

	/* LCD display initialization */
	display_init();

	/* LCD display information */
	ili9325_fill(COLOR_TURQUOISE);
	ili9325_draw_string(0, 20,
			(uint8_t *)"OV7740 image sensor\ncapture example");
	ili9325_draw_string(0, 80,
			(uint8_t *)"Please Wait during \ninitialization");

	/* Configure SMC interface for external SRAM. This SRAM will be used
	 * to store picture during image sensor acquisition.
	 */
	board_configure_sram();

	/* Configure push button to generate interrupt when the user press it */
	configure_button();

	/* OV7740 image sensor initialization*/
	capture_init();

	/* LCD display information*/
	ili9325_fill(COLOR_TURQUOISE);
	ili9325_draw_string(0, 20,
			(uint8_t *)"OV7740 image sensor\ncapture example");
	ili9325_draw_string(0, 80,
			(uint8_t *)"Please Press button\nto start processing\n barcodes");
	
	time_tick_init();

	while (1) {
			
		if (g_ul_push_button_trigger) {

			/* Capture a picture and send corresponding data to external
			 * memory */			
			g_ul_begin_capture_time = time_tick_get();
			start_capture();
			g_ul_end_capture_time = time_tick_get();
			
			/* Load picture data from external memory and display it on the
			 * LCD */
			_display();

			g_ul_begin_process_time = time_tick_get();

			zbar_hiku_process();
			IplImage *IgradX = 0;
			IgradX = cvCreateImage(cvSize(320,240),IPL_DEPTH_16S, 1);

//			zbar_image_scanner_t *scanner = NULL;
			/* create a reader */
//			scanner = zbar_image_scanner_create();
			/* configure the reader */
//			zbar_image_scanner_set_config(scanner, ZBAR_NONE, ZBAR_CFG_ENABLE, 1);
	
			//uint8_t * temp = teststring;	
			//zbar_image_t *image = zbar_image_create();
			//zbar_image_set_format(image, zbar_fourcc('Y','8','0','0'));
			//zbar_image_set_size(image, 6, 190);
			//zbar_image_set_data(image, temp, 6 * 190, zbar_image_free_data);

/*			uint8_t * temp = (uint8_t *)CAP_DEST;
			zbar_image_t *image = zbar_image_create();
			zbar_image_set_format(image, zbar_fourcc('G','R','E','Y'));
			zbar_image_set_size(image, IMAGE_WIDTH, IMAGE_HEIGHT);
			zbar_image_set_data(image, temp, IMAGE_WIDTH * IMAGE_HEIGHT, zbar_image_free_data);
*/

			/* scan the image for barcodes */
//			int n = zbar_scan_image(scanner, image);

			/* extract results */
//			const zbar_symbol_t *symbol = zbar_image_first_symbol(image);
/*			for(; symbol; symbol = zbar_symbol_next(symbol)) {
				
				g_ul_end_process_time = time_tick_get();

				char capture_time[32];
				char process_time[32];
				char total_time[32];
				
				sprintf(capture_time, "%u ms", g_ul_end_capture_time - g_ul_begin_capture_time);
				sprintf(process_time, "%u ms", g_ul_end_process_time - g_ul_begin_process_time);
				sprintf(total_time, "%u ms", time_tick_get() - g_ul_begin_capture_time);

				// print the results 
				zbar_symbol_type_t typ = zbar_symbol_get_type(symbol);
				volatile const char *data = zbar_symbol_get_data(symbol);
	
				//printf("decoded %s symbol \"%s\"\n", zbar_get_symbol_name(typ), data);
				display_init();
				ili9325_fill(COLOR_BLUE);
				ili9325_draw_string(0, 20, (uint8_t *)data);
				ili9325_draw_string(0, 80, (uint8_t *)zbar_get_symbol_name(typ));
				ili9325_draw_string(0, 100, (uint8_t *)capture_time );
				ili9325_draw_string(0, 120, (uint8_t *)process_time );
				ili9325_draw_string(0, 140, (uint8_t *)total_time );

				g_ul_push_button_trigger = false;


			}
*/
		} 
		
		if (g_display_splash) {
			display_init();
			ili9325_fill(COLOR_TOMATO);
			ili9325_draw_string(0, 80, (uint8_t *)"Press button\nto start decoding \nbrah!");
			g_display_splash = false;
		}

	}
}
