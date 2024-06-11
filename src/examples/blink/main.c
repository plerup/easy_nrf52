//====================================================================================
//
// blink
//
// Simple minimal test example without softdevice or bootloader
//
// This file is part of easy_nrf52
// License: LGPL 2.1
// General and full license information is available at:
//   https://github.com/plerup/easy_nrf52
//
// Copyright (c) 2024 Peter Lerup. All rights reserved.
//
//====================================================================================

#include <stdbool.h>
#include <stdint.h>
#include "nrf_delay.h"
#include "boards.h"

#define LED LED_1

int main() {
  nrf_gpio_cfg_output(LED);
  int32_t state = 0;
  while (true) {
    nrf_gpio_pin_write(LED, (state++) % 2);
    nrf_delay_ms(500);
  }
}