
//====================================================================================
//
// ibeacon
//
// Simple ibeacon example for easy_nrf52
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

#define ADV_INTERVAL_MS 1000
uint8_t ibeacon_data[] =
    {
        0x02,                    // Type
        0x15,                    // Size
        0x01, 0x02, 0x03, 0x04,  // UUID
        0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C,
        0x0D, 0x0E, 0x0F, 0x10,
        0x00, 0x01,             // Major
        0x00, 0x02,             // Minor
        0xC3                    // RSSI
};

int main() {
  enrf_init("ibeacon", NULL);
  enrf_start_advertise(false, 0x004C, BLE_ADVDATA_NO_NAME,
                       ibeacon_data, sizeof(ibeacon_data),
                       ADV_INTERVAL_MS, 0, NULL);
  while (true)
    enrf_wait_for_event();

}