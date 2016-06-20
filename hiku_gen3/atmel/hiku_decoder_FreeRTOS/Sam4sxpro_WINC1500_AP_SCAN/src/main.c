//#include "stdio.h"
//#include "stdlib.h"

#include "asf.h"
#include <string.h>
#include "main.h"


#ifndef PIO_PCMR_DSIZE_WORD
#  define PIO_PCMR_DSIZE_WORD PIO_PCMR_DSIZE(2)
#endif

#define STRING_EOL    "\r\n"
#define STRING_HEADER "-- SM2010 Example --"STRING_EOL \
	"-- "BOARD_NAME " --"STRING_EOL	\
	"-- Compiled: "__DATE__ " "__TIME__ " --"STRING_EOL
	
/** Index of scan list to request scan result. */
static uint8_t scan_request_index = 0;
/** Number of APs found. */
static uint8_t num_founded_ap = 0;

/**
 * \brief Configure UART console.
 */
static void configure_console(void)
{
	const usart_serial_options_t uart_serial_options = {
		.baudrate =		CONF_UART_BAUDRATE,
		.charlength =	CONF_UART_CHAR_LENGTH,
		.paritytype =	CONF_UART_PARITY,
		.stopbits =		CONF_UART_STOP_BITS,
	};

	/* Configure UART console. */
	sysclk_enable_peripheral_clock(CONSOLE_UART_ID);
	stdio_serial_init(CONF_UART, &uart_serial_options);
}

/* ---------------- Camera related fcns --------------------*/

static volatile uint32_t g_ul_vsync_flag = false;

uint8_t cap_dest_buf[76800];
volatile uint8_t *g_p_uc_cap_dest_buf = &cap_dest_buf[0];

//int *iArr = (int*)malloc(1000*sizeof(int));

uint16_t g_us_cap_line = 320;
uint16_t g_us_cap_rows = 240;

static void vsync_handler(uint32_t ul_id, uint32_t ul_mask)
{
	unused(ul_id);
	unused(ul_mask);

	g_ul_vsync_flag = true;
}

static void init_vsync_interrupts(void)
{
	/* Initialize PIO interrupt handler, see PIO definition in conf_board.h */
	pio_handler_set(PIOA, ID_PIOA, PIO_PA15, PIO_DEFAULT, vsync_handler);

	/* Enable PIO controller IRQs */
	NVIC_EnableIRQ((IRQn_Type)ID_PIOA);
}

static void pio_capture_init(Pio *p_pio, uint32_t ul_id)
{
	/* Enable peripheral clock */
	pmc_enable_periph_clk(ul_id);

	/* Disable pio capture so that DSIZE,ALWYS,HALFS and FRSTS can be changed */
	p_pio->PIO_PCMR &= ~((uint32_t)PIO_PCMR_PCEN);

	/* Disable rxbuff interrupt */
	p_pio->PIO_PCIDR |= PIO_PCIDR_RXBUFF;

	/* 32bit width*/
	p_pio->PIO_PCMR &= ~((uint32_t)PIO_PCMR_DSIZE_Msk);
	p_pio->PIO_PCMR |= PIO_PCMR_DSIZE_WORD;

	/* Only HSYNC and VSYNC enabled */
	p_pio->PIO_PCMR &= ~((uint32_t)PIO_PCMR_ALWYS);	//set ALWYS = 0
	p_pio->PIO_PCMR &= ~((uint32_t)PIO_PCMR_HALFS);

	//#if !defined(DEFAULT_MODE_COLORED)
	/* Samples only data with even index */
	p_pio->PIO_PCMR |= PIO_PCMR_HALFS;				//set HALFS = 1
	p_pio->PIO_PCMR &= ~((uint32_t)PIO_PCMR_FRSTS);	//set FRSTS = 0
	//#endif
}

/**
 * \brief Capture OV7740 data to a buffer.
 *
 * \param p_pio PIO instance which will capture data from OV7740 iamge sensor.
 * \param p_uc_buf Buffer address where captured data must be stored.
 * \param ul_size Data frame size.
 */
static uint8_t pio_capture_to_buffer(Pio *p_pio, uint8_t *uc_buf, uint32_t ul_size)
{
	/* Check if the first PDC bank is free */
	if ((p_pio->PIO_RCR == 0) && (p_pio->PIO_RNCR == 0)) {
		p_pio->PIO_RPR = (uint32_t)uc_buf;
		p_pio->PIO_RCR = ul_size;
		p_pio->PIO_PTCR = PIO_PTCR_RXTEN;
		printf("pio_capture_free_1\r\n");
		return 1;
	} else if (p_pio->PIO_RNCR == 0) {
		p_pio->PIO_RNPR = (uint32_t)uc_buf;
		p_pio->PIO_RNCR = ul_size;
		printf("pio_capture_free_2\r\n");	
		return 1;
	} else {
		printf("pdc not configured\r\n");	
		return 0;
	}
}

/* ---------------- Register Read/Write --------------------*/

typedef struct _cmos_reg {
	uint8_t reg; /*!< Register to be written */
	uint8_t val; /*!< Value to be written in the register */
} cmos_reg;

const cmos_reg SM2010_QVGA_CONF[] = {
	{0x12,0x80},
	{0x09,0x02},
	{0x15,0x02},
	{0x12,0x10},
	{0x1e,0x20},
	{0x13,0x00},
	{0x01,0x14},
	{0x02,0x21},
	{0x8c,0x02},
	{0x8d,0x64},
	{0x87,0x18},
	{0x13,0x07},
	{0x11,0x80},
	{0x2b,0x20},
	{0x92,0x40},
	{0x9d,0x99},
	{0xeb,0x30},
	{0xbb,0x20},
	{0xf5,0x21},
	{0xe1,0X3c},
	{0x16,0x03},
	{0x2f,0Xf6},
	{0x33,0x20},
	{0x34,0x08},
	{0x35,0x50},
	{0x65,0x4a},
	{0x66,0x50},
	{0x36,0x05},
	{0x37,0xf6},
	{0x38,0x46},
	{0x9b,0xf6},
	{0x9c,0x46},
	{0xbc,0x01},
	{0xbd,0xf6},
	{0xbe,0x46},
	{0x82,0x14},
	{0x83,0x23},
	{0x9a,0x23},
	{0x70,0x6f},
	{0x72,0x3f},
	{0x73,0x3f},
	{0x74,0x27},
	{0x77,0x90},
	{0x7a,0x1e},
	{0x7b,0x30},
	{0x84,0x1a},
	{0x85,0x20},
	{0x89,0x02},
	{0x8a,0x64},
	{0x86,0x30},
	{0x96,0xa6},
	{0x97,0x0c},
	{0x98,0x18},
	{0x80,0x55},
	{0x24,0x70},
	{0x25,0x80},
	{0x69,0x00},
	{0x94,0x0a},
	{0x1F,0x20},
	{0x22,0x20},
	{0x26,0x20},
	{0x56,0x40},
	{0x61,0xd3},
	{0x79,0x48},
	{0x3b,0x60},
	{0x3c,0x20},
	{0x39,0x80},
	{0x3f,0xb0},
	{0x39,0x80},
	{0x40,0X58},
	{0x41,0X54},
	{0x42,0X4E},
	{0x43,0X44},
	{0x44,0X3E},
	{0x45,0X39},
	{0x46,0X35},
	{0x47,0X31},
	{0x48,0X2E},
	{0x49,0X2B},
	{0x4b,0X29},
	{0x4c,0X27},
	{0x4e,0X23},
	{0x4f,0X20},
	{0x50,0X1e},
	{0x51,0x05},
	{0x52,0x10},
	{0x53,0x0b},
	{0x54,0x15},
	{0x57,0x87},
	{0x58,0x72},
	{0x59,0x5f},
	{0x5a,0x7e},
	{0x5b,0x1f},
	{0x5c,0x0e},
	{0x5d,0x95},
	{0x60,0x28},
	{0xb0,0xe0},
	{0xb1,0xc0},
	{0xb2,0xb0},
	{0xb3,0x88},
	{0x6a,0x01},
	{0x23,0x66},
	{0xa0,0x03},
	{0x06,0xe0},
	{0xa1,0X31},
	{0xa2,0X0b},
	{0xa3,0X26},
	{0xa4,0X05},
	{0xa5,0x25},
	{0xa6,0x06},
	{0xa7,0x80},
	{0xa8,0x80},
	{0xa9,0x20},
	{0xaa,0x20},
	{0xab,0x20},
	{0xac,0x3c},
	{0xad,0xf0},
	{0xc8,0x18},
	{0xc9,0x20},
	{0xca,0x17},
	{0xcb,0x1f},
	{0xaf,0x00},
	{0xc5,0x18},
	{0xc6,0x00},
	{0xc7,0x20},
	{0xae,0x80},
	{0xcc,0x40},
	{0xcd,0x58},
	{0xee,0x4c},
	{0x8e,0x07},
	{0x8f,0x79},
	{0xFF, 0xFF}
};

const cmos_reg *p_cmos_conf = SM2010_QVGA_CONF;


		
uint32_t cmos_write_regs(Twi* const p_twi, const cmos_reg *p_reg_list)
{
	uint32_t ul_err;
	uint32_t ul_size = 0;
	twi_packet_t twi_packet_regs;
	cmos_reg *p_next = (cmos_reg *)p_reg_list;
	
	printf("Writing regs\r\n");

	while (!((p_next->reg == 0xff) && (p_next->val == 0xff))) {
		//if (p_next->reg == 0xFD) {
		//	delay_ms(5);
		//} else {
			twi_packet_regs.addr[0] = p_next->reg;
			twi_packet_regs.addr_length = 1;
			twi_packet_regs.chip = 0x6e;
			twi_packet_regs.length = 1;
			twi_packet_regs.buffer = &(p_next->val);

			ul_err = twi_master_write(TWI0, &twi_packet_regs);
			delay_ms(5);
			//printf("[%x]: %x\r\n", twi_packet_regs.addr[0], p_next->val);
			ul_size++;

			if (ul_err == TWI_BUSY) {
				printf("error writing TWI\r\n");
				return ul_err;
			}
		//}

		p_next++;
	}
	printf("Done writing\r\n");
	return 0;
}


/**
 * \brief Main application function.
 *
 * Application entry point.
 *
 * \return program return value.
 */
int main(void)
{
	//tstrWifiInitParam param;
	int8_t ret;

	/* Initialize the board. */
	sysclk_init();
	board_init();

	/* Initialize the UART console. */
	configure_console();
	printf(STRING_HEADER);

	//clk
	//pmc_enable_pllack(7, 0x1, 1); /* PLLA work at 96 Mhz */

	//temp hack
	//ioport_set_pin_level( IOPORT_CREATE_PIN(PIOC, 24), 1);
	//delay_ms(50);
	//ioport_set_pin_level( IOPORT_CREATE_PIN(PIOC, 24), 0);
	//delay_ms(10);
	//ioport_set_pin_level( IOPORT_CREATE_PIN(PIOC, 24), 1);
		
	/*--------------- Initialize the TWI ---------------	*/
	twi_options_t opt;
	/* Enable TWI peripheral */
	pmc_enable_periph_clk(ID_TWI0);

	/* Init TWI peripheral */
	opt.master_clk = sysclk_get_cpu_hz();
	opt.speed  = (400000UL);
	opt.chip = 0x6e;
	twi_master_init(TWI0, &opt);

	/* Configure TWI interrupts */
	NVIC_DisableIRQ(TWI0_IRQn);
	NVIC_ClearPendingIRQ(TWI0_IRQn);
	NVIC_SetPriority(TWI0_IRQn, 0);
	NVIC_EnableIRQ(TWI0_IRQn);

	if (twi_probe(TWI0, 0x6e) == TWI_SUCCESS) {
		printf("cmos sensor found\r\n");
	} else {
		printf("not found\r\n");
	}
	
	// set to YUV422 QVGA mode
	//cmos_write_regs( TWI0, p_cmos_conf );

	/*	dump all cmos registers	*/
	twi_packet_t twi_cmos_packet;
	uint8_t cmos_reg_buf;
	uint32_t cmos_read_status;
	uint32_t i;
	
	twi_cmos_packet.addr_length = 1;
	twi_cmos_packet.chip = 0x6e;
	twi_cmos_packet.length = 1;
	twi_cmos_packet.buffer = &cmos_reg_buf;
	
	for (i = 0; i <= 0xfd; i++){
		twi_cmos_packet.addr[0] = i;
		cmos_read_status = twi_master_read(TWI0, &twi_cmos_packet);
		
		if (cmos_read_status == TWI_SUCCESS){
			printf("[%x]: %x\r\n", i, cmos_reg_buf);
		} else {
			printf("%x = NA\r\n", i);
		}		
		delay_ms(5);
	}
	uint8_t cmos_val = 0x02;
	twi_cmos_packet.addr[0] = 0x09;	
	twi_cmos_packet.buffer = &cmos_val;
	twi_master_write(TWI0, &twi_cmos_packet);
	delay_ms(5);
	
	cmos_read_status = twi_master_read(TWI0, &twi_cmos_packet);
	printf("read value [%x]: %x\r\n", twi_cmos_packet.addr[0], cmos_val);
	delay_ms(50);
	
	// set to YUV422 QVGA mode
	cmos_write_regs( TWI0, p_cmos_conf );

	//clk
	/* Init PCK0 to work at 24 Mhz */
	/* 96/4=24 Mhz */
	//PMC->PMC_PCK[0] = (PMC_PCK_PRES_CLK_4 | PMC_PCK_CSS_PLLA_CLK);
	//PMC->PMC_SCER = PMC_SCER_PCK0;
	//while (!(PMC->PMC_SCSR & PMC_SCSR_PCK0)) {
	//}


	delay_ms(3000);

	/*--------------- Initialize Camera ---------------	*/
	
	volatile uint8_t *p_uc_data = (uint8_t *)g_p_uc_cap_dest_buf;

	//turn on CMOS LED
	//ioport_set_pin_level( IOPORT_CREATE_PIN(PIOC, 24), 1);
	
	//	init vsync interrupts	
	init_vsync_interrupts();
	
	/*	PIODC Config	*/
	
	//1. Write PIO_PCIDR and PIO_PCIER in order to configure the parallel capture mode
	//interrupt mask.
	//-----pio_capture_init()

	//3. Write PIO_PCMR to set the fields DSIZE, ALWYS, HALFS and FRSTS in order to
	//configure the parallel capture mode WITHOUT enabling the parallel capture mode.
	//-----pio_capture_init()
	pio_capture_init(PIOA, ID_PIOA);
	
	//enable vsync interrupt and wait in while loop. 
	//after vsync interrupt, disable vsync interrupt. 
	
	pio_enable_interrupt(PIOA, PIO_PA15);
	while (!g_ul_vsync_flag) {
	}
	printf("disabling vsync\r\n");
	pio_disable_interrupt(PIOA, PIO_PA15);
	
	//4. Write PIO_PCMR to set PCEN bit to one in order to enable the parallel capture mode
	//WITHOUT changing the previous configuration.
	//-----pio_capture_enable (lib)
	
	pio_capture_enable(PIOA);
	
	//2. Configure PDC transfer in PDC registers.
	//g_p_uc_cap_dest_buf = (uint8_t *)CAP_DEST;
	pio_capture_to_buffer(PIOA, g_p_uc_cap_dest_buf, (g_us_cap_line*g_us_cap_rows)>>2 );
	//(g_us_cap_line*g_us_cap_rows)>>2
	
	//5. Wait for end of transfer by waiting for the interrupt corresponding to the flag ENDRX in
	//PIO_PCISR.
	//6. Check OVRE flag in PIO_PCISR.
	//-----while loop to wait end of capture
	
	/* Wait end of capture*/
	while (!((PIOA->PIO_PCISR & PIO_PCIMR_RXBUFF) == PIO_PCIMR_RXBUFF)) {
	}	
	printf("frame capture done\r\n");

	/* Disable pio capture*/
	pio_capture_disable(PIOA);

	/* Reset vsync flag*/
	g_ul_vsync_flag = false;

	//7. If a new buffer transfer is expected, go to step 5.
	//8. Write PIO_PCMR to set the PCEN bit to zero in order to disable the parallel capture
	//mode WITHOUT changing the previous configuration.
	//	

	printf("entering forever loop\r\n");
	
	uint32_t ul_cursor;
	for (ul_cursor = 76800; ul_cursor != 0; ul_cursor -= 1, p_uc_data += 1) {
		printf("%#02x,", *p_uc_data);
	}
	
	printf("busy waiting\r\n");
	while (1) {
		/* Handle pending events from network controller. */
		//while (m2m_wifi_handle_events(NULL) != M2M_SUCCESS) {
		//}
	}

	return 0;
}
