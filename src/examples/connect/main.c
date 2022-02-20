//====================================================================================
//
// connect
//
// BLE central connection example for easy_nrf52
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

// Search for a device running the advertiser example,
// connect and query version when found
#define PERIPH_NAME "Advertiser"
#define COMMAND     "Version?"

enum { IDLE, CONNECT, DISCONNECT } m_state = IDLE;
ble_gap_addr_t m_client_addr;

void nus_c_rx_cb(uint8_t *data, uint32_t length) {
  if (data) {
    static char str[100];
    strlcpy(str, (const char *)data, MIN(sizeof(str), length + 1));
    NRF_LOG_INFO("Response: %s", str);
    m_state = DISCONNECT;
  } else if (length == 1) {
    enrf_nus_c_string_send(COMMAND);
  }
}

//--------------------------------------------------------------------------

bool report_cb(ble_gap_evt_adv_report_t *p_adv_report) {
  uint8_t name[32];
  uint8_t name_len = enrf_adv_parse(p_adv_report,
                                    BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME,
                                    name, sizeof(name));
  name[name_len] = 0;
  if (strcmp((char*)name, PERIPH_NAME) == 0) {
    m_client_addr = p_adv_report->peer_addr;
    m_state = CONNECT;
    return true;
  }
  return false;
}

//--------------------------------------------------------------------------

void bsp_event_handler(bsp_event_t event) {
  bool long_range = event == BSP_EVENT_KEY_1;
  switch (event) {
    case BSP_EVENT_KEY_0:
    case BSP_EVENT_KEY_1:
      enrf_set_phy(long_range);
      enrf_start_scan(report_cb, 10, false);
      NRF_LOG_INFO("-- Start %s scanning --", long_range ? "long range (PHY_CODED)" : "normal");
      break;

    default:
      break;
  }
}

//--------------------------------------------------------------------------

int main() {
  enrf_init("connect", NULL);
  bsp_init(BSP_INIT_BUTTONS | BSP_INIT_LEDS, bsp_event_handler);
  enrf_connect_to(NULL, NULL, nus_c_rx_cb);
  while (true) {
    enrf_wait_for_event();
    if (m_state == CONNECT) {
      m_state = IDLE;
      enrf_stop_scan();
      NRF_LOG_INFO("Trying to connect to: %s", enrf_addr_to_str(&m_client_addr));
      enrf_connect_to(&m_client_addr, NULL, nus_c_rx_cb);
    } else if (m_state == DISCONNECT) {
      m_state = IDLE;
      enrf_disconnect();
    }
  }
}