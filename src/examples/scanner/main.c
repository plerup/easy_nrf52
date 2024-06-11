//====================================================================================
//
// scanner
//
// BLE device scanner example for easy_nrf52
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

bool m_scanning = false;

bool report_cb(ble_gap_evt_adv_report_t *p_adv_report) {
  // Print information about a found device if it has a name
  uint8_t name[32];
  uint8_t name_len = enrf_adv_parse(p_adv_report,
                                    BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME,
                                    name, sizeof(name));
  if (!name_len) {
    return false;
  }
  name[name_len] = 0;
  static char info[100];
  snprintf(info, sizeof(info), "%s;%s;%d", enrf_addr_to_str(&(p_adv_report->peer_addr)), name,
           p_adv_report->rssi);
  NRF_LOG_INFO("%s", info);
  return false;
}

//--------------------------------------------------------------------------

void bsp_event_handler(bsp_event_t event) {
  // Start or stop scanning based on board button press
  bool long_range = event == BSP_EVENT_KEY_1;
  switch (event) {
    case BSP_EVENT_KEY_0:
    case BSP_EVENT_KEY_1:
      if (m_scanning) {
        enrf_stop_scan();
        NRF_LOG_INFO("-- Stop scanning --");
      } else {
        enrf_set_phy(long_range);
        enrf_start_scan(report_cb, 0, true);
        NRF_LOG_INFO("-- Start %s scanning --", long_range ? "long range (PHY_CODED)" : "normal");
      }
      m_scanning = !m_scanning;
      SET_LED(BSP_BOARD_LED_0, m_scanning && !long_range);
      SET_LED(BSP_BOARD_LED_1, m_scanning && long_range);
      break;

    default:
      break;
  }
}

//--------------------------------------------------------------------------

int main() {
  enrf_init("scanner", NULL);
  bsp_init(BSP_INIT_BUTTONS | BSP_INIT_LEDS, bsp_event_handler);
  while (true) {
    enrf_wait_for_event();
  }
}