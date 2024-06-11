//====================================================================================
//
// enrf.h
//
// Simplified abstraction layer for the Nordic SDK
//
// This file is part of easy_nrf52
// License: LGPL 2.1
// General and full license information is available at:
//   https://github.com/plerup/easy_nrf52
//
// Copyright (c) 2022-2024 Peter Lerup. All rights reserved.
//
//====================================================================================

#ifndef ENRF_BLE_H
#define ENRF_BLE_H

#define _GNU_SOURCE

#include "sdk_common.h"

#include "ble_advdata.h"
#include "ble_db_discovery.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_log.h"
#include "nrf_sdh_ble.h"
#include "nrf_delay.h"
#include "boards.h"
#include "bsp_btn_ble.h"

#include "build_info.h"

#define SET_LED(led, on) if (on) bsp_board_led_on(led); else bsp_board_led_off(led);

#ifdef __cplusplus
extern "C" {
#endif

// Optional callback definitions
typedef bool (*nus_rx_cb_t)(uint8_t *data, uint32_t length);
typedef bool (*scan_report_cb_t)(ble_gap_evt_adv_report_t *);
typedef void (*db_disc_cb_t)(ble_db_discovery_evt_t *p_evt);
typedef void (*nus_c_rx_cb_t)(uint8_t *data, uint32_t length);

// Initiate the BLE stack
bool enrf_init(const char *dev_name, nrf_sdh_ble_evt_handler_t ble_evt_cb);

// Set PHY and tx power for all subsequent operations
void enrf_set_phy(bool long_range);
void enrf_set_tx_power(int tx_power);

//== Peripheral role functions ==

ret_code_t enrf_start_advertise(bool connectable,
                                uint16_t company_id, ble_advdata_name_type_t type,
                                uint8_t *p_data, uint8_t size,
                                uint32_t interval_ms, uint32_t timeout_s,
                                nus_rx_cb_t nus_cb);
ret_code_t enrf_stop_advertise();

bool enrf_is_connected();
ret_code_t enrf_disconnect();

// Send data from NUS server
ret_code_t enrf_nus_data_send(const uint8_t *data, uint32_t length);
ret_code_t enrf_nus_string_send(const char *str);

//== Central role functions ==

// Scan for peripheral devices
void enrf_set_scan_par(uint16_t scan_int, uint16_t scan_wind);
ret_code_t enrf_start_scan(scan_report_cb_t report_cb, uint32_t timeout_s, bool active);
ret_code_t enrf_stop_scan();
// Get the data of the first one of the specified fields in an advertisement package
uint8_t enrf_adv_parse(ble_gap_evt_adv_report_t *p_adv_report, uint8_t start_tag, uint8_t end_tag,
                       uint8_t *dest, uint8_t dest_len);

// Set parameters for next connection
void enrf_set_connection_params(float min_con_int_ms, float max_con_int_ms, uint16_t slave_latency,
                                float sup_timeout_ms);
// Connect and optionally initiate as a Nordic UART client
ret_code_t enrf_connect_to(ble_gap_addr_t *addr, db_disc_cb_t disc_cb, nus_c_rx_cb_t nus_c_rx_cb);
// Enable notifications on the specified characteristics cccd handle
ret_code_t enrf_enable_char_notif(uint16_t cccd_handle, bool enable);
// Write characteristics data using the specified write-op
ret_code_t enrf_write_char(uint8_t op, uint16_t char_handle, uint8_t *data, uint16_t length);
// Read characteristics data
ret_code_t enrf_read_char(uint16_t char_handle);

// Send data from the NUS client
ret_code_t enrf_nus_c_data_send(const uint8_t *data, uint32_t length);
ret_code_t enrf_nus_c_string_send(const char *str);

//== Utility functions ==

// Standard idle function
void enrf_wait_for_event();

// Restart the device with possible option to enter dfu mode (when bootloader present)
void enrf_restart(bool enter_dfu);

// Get string representation of BLE mac address
const char *enrf_addr_to_str(ble_gap_addr_t *gap_addr);
// And the other way around
bool enrf_str_to_addr(const char *addr_str, ble_gap_addr_t *gap_addr);

// Get the mac address of the current device as a string
const char *enrf_get_device_address();

// Serial string I/O to uart or usb. Activated via make variable ENRF_SERIAL
ret_code_t enrf_serial_enable(bool on);
ret_code_t enrf_serial_write(const char *str);
size_t enrf_serial_read(char *str, size_t max_length);

// Utility functions

// Convert hex string to byte array
uint32_t hex_to_bytes(const char *str, uint8_t *bytes, uint32_t max_len);

// Convert byte array to hex string
void bytes_to_hex(uint8_t *bytes, uint32_t len, char *str);

#ifdef __cplusplus
}
#endif

#endif