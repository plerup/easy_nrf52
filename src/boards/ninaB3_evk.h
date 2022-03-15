//
// challenger_840
//
// Board definition for Invector Challenger 840 board
//
//

#ifndef BOARD_NINAB3_EVK_H
#define BOARD_NINAB3_EVK_H

#include "nrf_gpio.h"

#define LEDS_NUMBER 3

#define LED_1 NRF_GPIO_PIN_MAP(0, 25)
#define LED_2 NRF_GPIO_PIN_MAP(0, 13)
#define LED_3 NRF_GPIO_PIN_MAP(1, 0)

#define LEDS_ACTIVE_STATE 0

#define LEDS_LIST { LED_1, LED_2, LED_3 }

#define LEDS_INV_MASK  !LEDS_MASK

#define BSP_LED_0      LED_1
#define BSP_LED_1      LED_2
#define BSP_LED_2      LED_3

#define BUTTONS_NUMBER 1

#define BUTTON_1       NRF_GPIO_PIN_MAP(0, 2)
#define BUTTON_PULL    NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST { BUTTON_1 }

#define BSP_BUTTON_0   BUTTON_1

#define TX_PIN_NUMBER NRF_GPIO_PIN_MAP(1, 13)
#define RX_PIN_NUMBER NRF_GPIO_PIN_MAP(0, 29)

#endif  // BOARD_NINAB3_EVK_H
