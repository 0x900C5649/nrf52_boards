/**
 * Copyright (c) 2016 - 2018, Nordic Semiconductor ASA
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 * 
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 * 
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 * 
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
#ifndef CUSTOM_BOARD_H
#define CUSTOM_BOARD_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "nrf_gpio.h"

#define A0      NRF_GPIO_PIN_MAP(0, 4)
#define A1      NRF_GPIO_PIN_MAP(0, 5)
#define A2      NRF_GPIO_PIN_MAP(0, 30)
#define A3      NRF_GPIO_PIN_MAP(0, 28)
#define A4      NRF_GPIO_PIN_MAP(0, 2)
#define A5      NRF_GPIO_PIN_MAP(0, 3)

#define D2      NRF_GPIO_PIN_MAP(0, 10)
#define NC      D2
#define D5      NRF_GPIO_PIN_MAP(1, 8)
#define D6      NRF_GPIO_PIN_MAP(0, 7)
#define D9      NRF_GPIO_PIN_MAP(0, 26)
#define D10     NRF_GPIO_PIN_MAP(0, 27)
#define D11     NRF_GPIO_PIN_MAP(0, 6)
#define D12     NRF_GPIO_PIN_MAP(0, 8)
#define D13     NRF_GPIO_PIN_MAP(1, 9)

#define SDA     NRF_GPIO_PIN_MAP(0, 22)
#define SCL     NRF_GPIO_PIN_MAP(0, 22)

#define TX      NRF_GPIO_PIN_MAP(0, 11)
#define RX      NRF_GPIO_PIN_MAP(0, 12)

#define MISO    NRF_GPIO_PIN_MAP(0, 15)
#define MOSI    NRF_GPIO_PIN_MAP(0, 13)
#define SCK     NRF_GPIO_PIN_MAP(0, 14)

#define NEOPIX      NRF_GPIO_PIN_MAP(0, 16)

#define QSPI_SCK    NRF_GPIO_PIN_MAP(0, 19)
#define QSPI_DATA0  NRF_GPIO_PIN_MAP(0, 17)
#define QSPI_DATA1  NRF_GPIO_PIN_MAP(0, 22)
#define QSPI_CS     NRF_GPIO_PIN_MAP(0, 20)
#define QSPI_DATA2  NRF_GPIO_PIN_MAP(0, 23)
#define QSPI_DATA3  NRF_GPIO_PIN_MAP(0, 21)

#define SWO         NRF_GPIO_PIN_MAP(1, 0)

#define SWITCH      NRF_GPIO_PIN_MAP(1, 2)


// LEDs definitions for nRF52840-MDK
#define LEDS_NUMBER 2

#define LED_START LED_1
#define LED_1     NRF_GPIO_PIN_MAP(1, 15)
#define LED_RED   LED1
#define LED_2     NRF_GPIO_PIN_MAP(1, 10)
#define LED_BLUE  LED_2
#define LED_STOP  LED_2

#define LEDS_ACTIVE_STATE 1
#define LEDS_ACTIVE_HIGH true

#define LEDS_LIST           \
    {                       \
        LED_1, LED_2        \
    }

#define LEDS_INV_MASK 0

#define BSP_LED_0 LED_1
#define BSP_LED_1 LED_2


#define BUTTONS_NUMBER 1

#define BUTTON_0    SWITCH
#define BUTTON_PULL NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST \
    {                \
        BUTTON_0     \
    }

#define BSP_BUTTON_0 BUTTON_0


#define BTN_USER 0

//#define CTS_PIN_NUMBER 7
//#define RTS_PIN_NUMBER 5
#define HWFC           false

#define BSP_QSPI_SCK_PIN    QSPI_SCK
#define BSP_QSPI_CSN_PIN    QSPI_CS
#define BSP_QSPI_IO0_PIN    QSPI_DATA0
#define BSP_QSPI_IO1_PIN    QSPI_DATA1
#define BSP_QSPI_IO2_PIN    QSPI_DATA2
#define BSP_QSPI_IO3_PIN    QSPI_DATA3


#ifdef USE_APP_BOARDCONFIG
#include STRINGIFY(BOARD/app_board_config.h)
#endif

#ifdef __cplusplus
}
#endif

#endif  // CUSTOM_BOARD_H

