/**
 * \file
 *
 * \brief Board configuration.
 *
 * Copyright (c) 2014-2015 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */
/*
 * Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
 */

#ifndef CONF_BOARD_H_INCLUDED
#define CONF_BOARD_H_INCLUDED

/** Enable AAT3155 component to control LCD backlight. */
//#define CONF_BOARD_AAT3155

/** Enable ILI9325 component to control LCD. */
#define CONF_BOARD_ILI9325

/** Enable OV7740 image sensor. */
#define CONF_BOARD_OV7740_IMAGE_SENSOR

/** Configure TWI0 pins (for OV7740  communications). */
//#define CONF_BOARD_TWI0

/** Configure PCK0 pins (for OV7740  communications). */
//#define CONF_BOARD_PCK0

/* Enable SRAM. */
#define CONF_BOARD_SRAM

/* TWI board defines. */
#define ID_BOARD_TWI                   ID_TWI0
#define BOARD_TWI                      TWI0
#define BOARD_TWI_IRQn                 TWI0_IRQn

/* Push button board defines. */
#define PUSH_BUTTON_ID                 PIN_PUSHBUTTON_1_ID
#define PUSH_BUTTON_PIO                PIN_PUSHBUTTON_1_PIO
#define PUSH_BUTTON_PIN_MSK            PIN_PUSHBUTTON_1_MASK
#define PUSH_BUTTON_ATTR               PIN_PUSHBUTTON_1_ATTR

/* SRAM board defines. */
#define SRAM_BASE                      (0x60000000UL) // SRAM adress
#define SRAM_CS                        (0UL)
#define CAP_DEST                       SRAM_BASE

/* LCD board defines. */
#define ILI9325_LCD_CS                 (2UL) // Chip select number
#define IMAGE_WIDTH                    (320UL)
#define IMAGE_HEIGHT                   (240UL)


#ifdef CONF_BOARD_OV7740_IMAGE_SENSOR
/* Image sensor board defines. */
// Image sensor Power pin.
#define OV_POWER_PIO                   OV_SW_OVT_PIO
#define OV_POWER_MASK                  OV_SW_OVT_MASK
// Image sensor VSYNC pin.
#define OV7740_VSYNC_PIO	       OV_VSYNC_PIO
#define OV7740_VSYNC_ID		       OV_VSYNC_ID
#define OV7740_VSYNC_MASK              OV_VSYNC_MASK
#define OV7740_VSYNC_TYPE              OV_VSYNC_TYPE
// Image sensor data pin.
#define OV7740_DATA_BUS_PIO            OV_DATA_BUS_PIO
#define OV7740_DATA_BUS_ID             OV_DATA_BUS_ID
#endif /* CONF_BOARD_OV7740_IMAGE_SENSOR */


#ifdef CONF_BOARD_OV7740_IMAGE_SENSOR
/******************************* Image sensor definition
 *********************************/
/** pin for OV_VDD */
#define PIN_OV_SW_OVT                  { PIO_PC10, PIOC, ID_PIOC, \
                                         PIO_OUTPUT_1, PIO_DEFAULT} /* OV_VDD */
#define PIN_OV_RST                     { PIO_PC15, PIOC, ID_PIOC, \
                                         PIO_OUTPUT_1, PIO_DEFAULT}
#define PIN_OV_FSIN                    { PIO_PA21, PIOA, ID_PIOA, \
                                         PIO_OUTPUT_0, PIO_DEFAULT}

/** HSYNC pin */
#define PIN_OV_HSYNC                   { PIO_PA16, PIOA, ID_PIOA, \
                                         PIO_INPUT, PIO_PULLUP | \
                                         PIO_IT_RISE_EDGE }

/** VSYNC pin */
#define PIN_OV_VSYNC                   { PIO_PA15, PIOA, ID_PIOA, \
                                         PIO_INPUT, PIO_PULLUP | \
                                         PIO_IT_RISE_EDGE }

/** OV_SW_OVT pin definition */
#define OV_SW_OVT_GPIO                 PIO_PC10_IDX
#define OV_SW_OVT_FLAGS                (PIO_OUTPUT_1 | PIO_DEFAULT)
#define OV_SW_OVT_MASK                 PIO_PC10
#define OV_SW_OVT_PIO                  PIOC
#define OV_SW_OVT_ID                   ID_PIOC
#define OV_SW_OVT_TYPE                 PIO_OUTPUT_1

/** OV_RST pin definition */
#define OV_RST_GPIO                    PIO_PC15_IDX
#define OV_RST_FLAGS                   (PIO_OUTPUT_1 | PIO_DEFAULT)
#define OV_RST_MASK                    PIO_PC15
#define OV_RST_PIO                     PIOC
#define OV_RST_ID                      ID_PIOC
#define OV_RST_TYPE                    PIO_OUTPUT_1

/** OV_RST pin definition */
#define OV_FSIN_GPIO                   PIO_PA21_IDX
#define OV_FSIN_FLAGS                  (PIO_OUTPUT_0 | PIO_DEFAULT)
#define OV_FSIN_MASK                   PIO_PA21
#define OV_FSIN_PIO                    PIOA
#define OV_FSIN_ID                     ID_PIOA
#define OV_FSIN_TYPE                   PIO_OUTPUT_0

/** OV_HSYNC pin definition */
#define OV_HSYNC_GPIO                  PIO_PA16_IDX
#define OV_HSYNC_FLAGS                 (PIO_PULLUP | PIO_IT_RISE_EDGE)
#define OV_HSYNC_MASK                  PIO_PA16
#define OV_HSYNC_PIO                   PIOA
#define OV_HSYNC_ID                    ID_PIOA
#define OV_HSYNC_TYPE                  PIO_PULLUP

/** OV_VSYNC pin definition */
#define OV_VSYNC_GPIO                  PIO_PA15_IDX
#define OV_VSYNC_FLAGS                 (PIO_PULLUP | PIO_IT_RISE_EDGE)
#define OV_VSYNC_MASK                  PIO_PA15
#define OV_VSYNC_PIO                   PIOA
#define OV_VSYNC_ID                    ID_PIOA
#define OV_VSYNC_TYPE                  PIO_PULLUP

/** OV Data Bus pins */
#define OV_DATA_BUS_D2                 PIO_PA24_IDX
#define OV_DATA_BUS_D3                 PIO_PA25_IDX
#define OV_DATA_BUS_D4                 PIO_PA26_IDX
#define OV_DATA_BUS_D5                 PIO_PA27_IDX
#define OV_DATA_BUS_D6                 PIO_PA28_IDX
#define OV_DATA_BUS_D7                 PIO_PA29_IDX
#define OV_DATA_BUS_D8                 PIO_PA30_IDX
#define OV_DATA_BUS_D9                 PIO_PA31_IDX
#define OV_DATA_BUS_FLAGS              (PIO_INPUT | PIO_PULLUP)
#define OV_DATA_BUS_MASK               (0xFF000000UL)
#define OV_DATA_BUS_PIO                PIOA
#define OV_DATA_BUS_ID                 ID_PIOA
#define OV_DATA_BUS_TYPE               PIO_INPUT
#define OV_DATA_BUS_ATTR               PIO_DEFAULT

/** List of Image sensor definitions. */
#define PINS_OV                        PIN_OV_SW_OVT, PIN_PCK0, PIN_OV_RST
#endif /* CONF_BOARD_OV7740_IMAGE_SENSOR */

#endif /* CONF_BOARD_H_INCLUDED */
