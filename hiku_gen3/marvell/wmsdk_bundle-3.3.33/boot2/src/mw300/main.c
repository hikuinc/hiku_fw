/*
 * Copyright (C) 2011-2013, Marvell International Ltd.
 * All Rights Reserved.
 */

/* Standard includes. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <boot2.h>
#include <secure_boot2.h>

/* Driver includes. */
#include <lowlevel_drivers.h>

void setup_stack(void) __attribute__((naked));
void initialize_bss(void) __attribute__((used));

unsigned long *nvram_addr = (unsigned long *)NVRAM_ADDR;
unsigned long *sb_e = (unsigned long *)SB_ERR_ADDR;

#ifdef UART_DEBUG
#define MAX_MSG_LEN 127
static char uart_msg_buf_[MAX_MSG_LEN];

void uart_dbg(const char *format, ...)
{
	va_list args;

	/* Format the string */
	va_start(args, format);
	vsnprintf(uart_msg_buf_, MAX_MSG_LEN, &format[0], args);
	va_end(args);
	UART_WriteLine(UART0_ID, (uint8_t *)uart_msg_buf_);
}

void uart_cleanup(void)
{
	UART_Disable(UART0_ID);
	GPIO_PinMuxFun(GPIO_2, GPIO2_GPIO2);
	GPIO_PinMuxFun(GPIO_3, GPIO3_GPIO3);
	CLK_SystemClkSrc(CLK_RC32M);
	PMU_PowerDownWLAN();
}

static void uart_init(void)
{
	if (CLK_GetClkStatus(CLK_SFLL) == RESET) {
		/* Power up WLAN core */
		PMU_PowerOnWLAN();

		/* Request 38.4MHz clock from WLAN side */
		CLK_RefClkEnable(CLK_XTAL_REF);
		/* Wait for WLAN REF clock ready */
		while (CLK_GetClkStatus(CLK_XTAL_REF) == RESET)
			;

		/* Select 38.4MHz ref clock as system clock */
		CLK_SystemClkSrc(CLK_XTAL_REF);
	}

	/* UART0 GPIO domain assuming GPIO2/3 */
	PMU_PowerOnVDDIO(PMU_VDDIO_0);
	CLK_SetUARTClkSrc(CLK_UART_ID_0, CLK_UART_FAST);
	CLK_ModuleClkEnable(CLK_UART0);

	UART_CFG_Type uartCfgStruct = {115200, UART_DATABITS_8,
		DISABLE, UART_PARITY_NONE,
		ENABLE,
		DISABLE};

	UART_FifoCfg_Type fifoCftStruct = {DISABLE,
		DISABLE,
		DISABLE,
		DISABLE,
		UART_PERIPHERAL_BITS_32,
		DISABLE,
		UART_RXFIFO_BYTES_8,
		UART_TXFIFO_HALF_EMPTY
	};

	/* UART0 configuration */
	UART_Disable(UART0_ID);
	UART_Init(UART0_ID, &uartCfgStruct);
	UART_FifoConfig(UART0_ID, &fifoCftStruct);
	UART_IntMask(UART0_ID, UART_INT_ALL, MASK);
	UART_Enable(UART0_ID);

	GPIO_PinMuxFun(GPIO_2, GPIO2_UART0_TXD);
	GPIO_PinMuxFun(GPIO_3, GPIO3_UART0_RXD);
}
#endif

static void system_init(void)
{
	/* We need to ensure cache flush which gets triggered when we exit
	 * from reset has been completed before the flash controller is
	 * configured in 88MW300.
	 */
	volatile uint32_t cnt = 0;

	while (cnt < 0x200000) {
		if (FLASHC->FCCR.BF.CACHE_LINE_FLUSH == 0)
			break;
		cnt++;
	}
}

int main(void)
{
	system_init();
	writel(SYS_INIT, nvram_addr);
	writel(SB_ERR_NONE, sb_e);

	QSPI->CONF.BF.CLK_PRESCALE = 0;

	if (CLK_GetSystemClk() > 50000000) {
		/* Max QSPI0 Clock Frequency is 50MHz */
		CLK_ModuleClkDivider(CLK_QSPI0, 4);
		/* Max APB0 freq 50MHz */
		CLK_ModuleClkDivider(CLK_APB0, 2);
		/* Max APB1 freq 50MHz */
		CLK_ModuleClkDivider(CLK_APB1, 2);
	}

	/* Sample data on the falling edge.
	 * Sampling on rising edge does not
	 * meet timing requirements at 50MHz
	 */
	QSPI->TIMING.BF.CLK_CAPT_EDGE = 1;

	/* As per Winbond datasheet, it is always recommended to remove flash
	 * from continuous read mode after reset in case controller reset
	 * occured when flash was in continuous read mode.
	 */
	FLASH_ResetFastReadQuad();

	writel(readel(nvram_addr) | FLASH_INIT, nvram_addr);
	dbg("Version %s\r\n", BOOT2_VERSION);
	dbg("Hardware setup done...\r\n");
	boot2_main();
	/* boot2_main never returns */
	return 0;
}

void initialize_bss(void)
{
	/* Zero fill the bss segment. */
	memset(&_bss, 0, ((unsigned) &_ebss - (unsigned) &_bss));
#ifdef UART_DEBUG
	uart_init();
#endif
	/* Call the application's entry point. */
	main();
}

void setup_stack(void)
{
	/* Setup stack pointer */
	asm("ldr r3, =_estack");
	asm("mov sp, r3");
	initialize_bss();
}
