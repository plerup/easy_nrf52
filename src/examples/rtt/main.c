//====================================================================================
//
// template
//
// Simple RTT input output example
//
// This file is part of easy_nrf52
// License: LGPL 2.1
// General and full license information is available at:
//   https://github.com/plerup/easy_nrf52
//
// Copyright (c) 2026 Peter Lerup. All rights reserved.
//
//====================================================================================

#include <enrf.h>
#include <SEGGER_RTT.h>

#define USED_LED BSP_BOARD_LED_0

void handle_incoming(char *str) {
  if (strcasestr(str, "Hello")) {
    NRF_LOG_INFO("Hello from rtt example");
  } else if (strcasestr(str, "led")) {
    bsp_board_led_invert(USED_LED);
    NRF_LOG_INFO("LED is %s", bsp_board_led_state_get(USED_LED) ? "on" : "off");
  } else {
    NRF_LOG_ERROR("Unknown command: %s", str);
  }
}

//--------------------------------------------------------------------------

int main() {
  static char incoming[50] = {0};
  enrf_init("rtt example", NULL);
  bsp_init(BSP_INIT_LEDS, NULL);
  NRF_LOG_INFO("rtt example started");
  while (true) {
    if (SEGGER_RTT_HasData(0) != 0) {
      unsigned int read_cnt = SEGGER_RTT_Read(0, incoming, sizeof(incoming) - 1);
      incoming[read_cnt] = 0;
      handle_incoming(incoming);
    } else {
      enrf_delay_ms(100);
    }
  }
}