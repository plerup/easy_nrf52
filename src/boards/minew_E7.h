//
// minew_e7
//
// Board definition for Minew E7 board
//
//

#ifndef BOARD_MINEW_E7_H
#define BOARD_MINEW_E7_H

#include "nrf_gpio.h"

#define LEDS_NUMBER 2

#define LED_1 NRF_GPIO_PIN_MAP(0, 18)
#define LED_2 NRF_GPIO_PIN_MAP(0, 17)

#define LEDS_ACTIVE_STATE 1

#define LEDS_LIST { LED_1, LED_2 }

#define LEDS_INV_MASK  !LEDS_MASK

#define BSP_LED_0      LED_1
#define BSP_LED_1      LED_2

#define BUTTONS_NUMBER 1

#define BUTTON_1       NRF_GPIO_PIN_MAP(0, 13)
#define BUTTON_PULL    NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST { BUTTON_1 }

#define BSP_BUTTON_0   BUTTON_1

#define TX_PIN_NUMBER NRF_GPIO_PIN_MAP(0, 21)  // Available as PCB test point
#define RX_PIN_NUMBER NRF_GPIO_PIN_MAP(0, 22)

#endif  // BOARD_MINEW_E7_H
