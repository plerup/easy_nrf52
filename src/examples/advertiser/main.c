//====================================================================================
//
// advertiser
//
// BLE advertising example for easy_nrf52
//
// This file is part of easy_nrf52
// License: LGPL 2.1
// General and full license information is available at:
//   https://github.com/plerup/easy_nrf52
//
// Copyright (c) 2022-2024 Peter Lerup. All rights reserved.
//
//====================================================================================

#include <enrf.h>

#define ADV_INTERVAL_MS 100

bool m_advertising = false;

void bsp_event_handler(bsp_event_t event) {
  // Start or stop advertisement based on board button press
  bool long_range = event == BSP_EVENT_KEY_1;
  switch (event) {
    case BSP_EVENT_KEY_0:
    case BSP_EVENT_KEY_1:
      if (m_advertising) {
        enrf_stop_advertise();
        NRF_LOG_INFO("Stop advertising")
      } else {
        enrf_set_phy(long_range);
        enrf_start_advertise(true, 0, BLE_ADVDATA_FULL_NAME, NULL, 0, ADV_INTERVAL_MS, 0, NULL);
        NRF_LOG_INFO("Start %s advertising", long_range ? "long range (PHY_CODED)" : "normal");
      }
      m_advertising = !m_advertising;
      SET_LED(BSP_BOARD_LED_0, m_advertising && !long_range);
      SET_LED(BSP_BOARD_LED_1, m_advertising && long_range);
      break;

    default:
      break;
  }
}

//--------------------------------------------------------------------------

int main() {
  enrf_init("Advertiser", NULL);
  bsp_init(BSP_INIT_BUTTONS | BSP_INIT_LEDS, bsp_event_handler);
  while (true) {
    enrf_wait_for_event();
  }
}