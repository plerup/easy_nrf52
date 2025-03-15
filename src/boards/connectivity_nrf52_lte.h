//
// challenger_840
//
// Board definition for Invector connectivity_nrf52_lte
//

#ifndef BOARD_CON_NRF52_LTE_H
#define BOARD_CON_NRF52_LTE_H

#include "nrf_gpio.h"

#define LEDS_NUMBER 4

#define LED_1 NRF_GPIO_PIN_MAP(0, 27)

#define LEDS_ACTIVE_STATE 1

#define LEDS_LIST \
  { LED_1, LED_1, LED_1, LED_1 }

#define LEDS_INV_MASK !LEDS_MASK

#define BSP_LED_0 LED_1

#define BUTTONS_NUMBER 1

#define BUTTON_1 NRF_GPIO_PIN_MAP(1, 3)
#define BUTTON_PULL NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST \
  { BUTTON_1 }

#define BSP_BUTTON_0 BUTTON_1

#define TX_PIN_NUMBER        NRF_GPIO_PIN_MAP(0, 17)
#define RX_PIN_NUMBER        NRF_GPIO_PIN_MAP(0, 15)
#define RTS_PIN_NUMBER       NRF_GPIO_PIN_MAP(0, 19)
#define CTS_PIN_NUMBER       NRF_GPIO_PIN_MAP(0, 13)

#define BSP_QSPI_SCK_PIN     NRF_GPIO_PIN_MAP(1, 8)
#define BSP_QSPI_CSN_PIN     NRF_GPIO_PIN_MAP(0, 16)
#define BSP_QSPI_IO0_PIN     NRF_GPIO_PIN_MAP(0, 6)
#define BSP_QSPI_IO1_PIN     NRF_GPIO_PIN_MAP(0, 8)
#define BSP_QSPI_IO2_PIN     NRF_GPIO_PIN_MAP(1, 9)
#define BSP_QSPI_IO3_PIN     NRF_GPIO_PIN_MAP(0, 11)

#define PIN_NBIOT_PWR_ENABLE NRF_GPIO_PIN_MAP(1, 2)
#define PIN_NBIOT_WAKEUP     NRF_GPIO_PIN_MAP(1, 6)
#define PIN_NBIOT_RST        NRF_GPIO_PIN_MAP(1, 5)
#define PIN_LORA_PWR_ENABLE  NRF_GPIO_PIN_MAP(1, 7)
#define PIN_LORA_RST         NRF_GPIO_PIN_MAP(0, 12)

#endif  // BOARD_CON_NRF52_LTE_H
