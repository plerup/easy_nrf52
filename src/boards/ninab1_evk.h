//
// ninab1_evk
//
// Board definition for U-blox Nina B1 EVK
//
//

#ifndef BOARD_NINAB1_EVK_H
#define BOARD_NINAB1_EVK_H

#include "nrf_gpio.h"

#define LEDS_NUMBER 2

#define LED_1 8
#define LED_2 18

#define LEDS_ACTIVE_STATE 0

#define LEDS_LIST \
  { LED_1, LED_2 }

#define LEDS_INV_MASK !LEDS_MASK

#define BSP_LED_0 LED_1
#define BSP_LED_1 LED_2

#define BUTTONS_NUMBER 2

#define BUTTON_1 16
#define BUTTON_2 30
#define BUTTON_PULL NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST \
  { BUTTON_1, BUTTON_2 }

#define BSP_BUTTON_0 BUTTON_1
#define BSP_BUTTON_1 BUTTON_2

#define TX_PIN_NUMBER 6
#define RX_PIN_NUMBER 5

#endif  // BOARD_NINAB1_EVK_H
