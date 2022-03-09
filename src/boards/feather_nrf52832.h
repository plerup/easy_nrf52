//
// feather_nrf52832
//
// Board definition for Adafruit feather nrf52832 board
//
//

#ifndef BOARD_FEATHER_NRF52832_H
#define BOARD_FEATHER_NRF52832_H

#include "nrf_gpio.h"

#define LEDS_NUMBER 2

#define LED_1 17
#define LED_2 19

#define LEDS_ACTIVE_STATE 1

#define LEDS_LIST \
  { LED_1, LED_2 }

#define LEDS_INV_MASK !LEDS_MASK

#define BSP_LED_0 LED_1
#define BSP_LED_1 LED_2

// Select one pin for dummy button
#define BUTTONS_NUMBER 1

#define BUTTON_1 3
#define BUTTON_PULL NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST \
  { BUTTON_1 }

#define BSP_BUTTON_0 BUTTON_1

#define TX_PIN_NUMBER 4
#define RX_PIN_NUMBER 6

#endif  // BOARD_FEATHER_NRF52832_H