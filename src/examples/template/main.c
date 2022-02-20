//====================================================================================
//
// template
//
// Simple example template for easy_nrf52 BLE peripheral applications
// using Nordic uart service as communication channel
//
// This file is part of easy_nrf52
// License: LGPL 2.1
// General and full license information is available at:
//   https://github.com/plerup/easy_nrf52
//
// Copyright (c) 2022 Peter Lerup. All rights reserved.
//
//====================================================================================

#include <enrf.h>

#define ADV_INTERVAL_MS 100
#define USED_LED BSP_BOARD_LED_0

bool nus_data_received(uint8_t *data, uint32_t length) {
  // Data received from Nordic UART service, assume it's a command string
  char str[20];
  bool taken = true;
  strlcpy(str, (const char *)data, MIN(sizeof(str), length+1));
  if (strcasestr(str, "Hello")) {
    enrf_nus_string_send("Hello from enrf template");
  } else if (strcasestr(str, "led")) {
    bsp_board_led_invert(USED_LED);
    snprintf(str, sizeof(str), "LED is %s", bsp_board_led_state_get(USED_LED) ? "on" : "off");
    enrf_nus_string_send(str);
  } else if (strcasestr(str, "DATA")) {
    // Send raw data
    uint8_t buff[256];
    for (int i = 0; i < sizeof(buff); i++)
      buff[i] = i;
    enrf_nus_data_send(buff, sizeof(buff));
  } else {
    // Propagate to common command handler
    taken = false;
  }
  return taken;
}

//--------------------------------------------------------------------------

int main() {
  enrf_init("enrf template", NULL);
  bsp_init(BSP_INIT_LEDS, NULL);
  // Start advertising
  enrf_start_advertise(true,                      // Connectable
                       0, BLE_ADVDATA_FULL_NAME,  // No company ID, full name
                       NULL, 0,                   // No data
                       ADV_INTERVAL_MS, 0,        // No timeout
                       nus_data_received          // Callback for nus
                      );
  while (true) {
    enrf_wait_for_event();
  }
}