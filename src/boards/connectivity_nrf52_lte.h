//
// challenger_840
//
// Board definition for Invector connectivity_nrf52_lte
//

#ifndef BOARD_CON_NRF52_LTE_H
#define BOARD_CON_NRF52_LTE_H

#include "nrf_gpio.h"

#define LEDS_NUMBER 1

#define LED_1 NRF_GPIO_PIN_MAP(0, 27)

#define LEDS_ACTIVE_STATE 1

#define LEDS_LIST \
  { LED_1 }

#define LEDS_INV_MASK !LEDS_MASK

#define BSP_LED_0 LED_1

#define BUTTONS_NUMBER 1

#define BUTTON_1 NRF_GPIO_PIN_MAP(1, 3)
#define BUTTON_PULL NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST \
  { BUTTON_1 }

#define BSP_BUTTON_0 BUTTON_1

#define TX_PIN_NUMBER 17
#define RX_PIN_NUMBER 15

#endif  // BOARD_CON_NRF52_LTE_H
