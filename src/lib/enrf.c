//====================================================================================
//
// enrf.c
//
// Simplified abstraction layer for the Nordic SDK
// Much of the code herein is taken from the Nordic SDK examples
//
// This file is part of easy_nrf52
// License: LGPL 2.1
// General and full license information is available at:
//   https://github.com/plerup/easy_nrf52
//
// Copyright (c) 2022 Peter Lerup. All rights reserved.
//
//====================================================================================

#if defined(ENRF_SERIAL_USB) && defined(ENRF_SERIAL_UART)
  #error Both usb and uart can not be used for enrf_serial
#endif

#define NRF_LOG_MODULE_NAME enrf
#include "enrf.h"
NRF_LOG_MODULE_REGISTER();

#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "app_timer.h"
#include "ble_nus.h"
#include "ble_nus_c.h"
#include "app_uart.h"
#include "app_util_platform.h"
#include "nrf_pwr_mgmt.h"

#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#ifdef ENRF_SERIAL_USB
  #include "app_usbd.h"
  #include "app_usbd_cdc_acm.h"
  #include "app_usbd_core.h"
  #include "app_usbd_serial_num.h"
  #include "app_usbd_string_desc.h"

#elif defined(ENRF_SERIAL_UART)
#endif

#if BLE_DFU_ENABLED == 1
  #include "nrf_dfu_ble_svci_bond_sharing.h"
  #include "nrf_svci_async_function.h"
  #include "nrf_svci_async_handler.h"
  #include "ble_dfu.h"
  #include "nrf_bootloader_info.h"
#endif

#define APP_BLE_CONN_CFG_TAG            1
#define APP_BLE_OBSERVER_PRIO           3

// Global variables
BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT);
BLE_NUS_C_DEF(m_ble_nus_c);
NRF_BLE_GATT_DEF(m_gatt);
NRF_BLE_QWR_DEF(m_qwr);
#if SDK_VERSION >= 17
NRF_BLE_GQ_DEF(m_ble_gatt_queue, /**< BLE GATT Queue instance. */
               NRF_SDH_BLE_CENTRAL_LINK_COUNT,
               NRF_BLE_GQ_QUEUE_SIZE);
#endif

static ble_gap_adv_params_t m_adv_params;
static uint8_t m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
static uint8_t m_enc_advdata[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
static ble_gap_adv_data_t m_adv_data = {
  .adv_data = {
    .p_data = m_enc_advdata,
    .len = sizeof(m_enc_advdata)
  },
  .scan_rsp_data = {
    .p_data = NULL,
    .len = 0
  }
};

static ble_gap_conn_params_t m_connection_param = {
    MSEC_TO_UNITS(20, UNIT_1_25_MS),
    MSEC_TO_UNITS(75, UNIT_1_25_MS),
    0,
    MSEC_TO_UNITS(4000, UNIT_10_MS)
};

static ble_gap_scan_params_t m_scan_params = {
    .active = 0,
    .interval = 0x00A0,
    .window = 0x0050,
    .timeout = 0,
    .scan_phys = BLE_GAP_PHY_AUTO
};

static uint8_t    m_scan_buffer[BLE_GAP_SCAN_BUFFER_EXTENDED_MIN];
static ble_data_t m_adv_rep_buffer = {.p_data = m_scan_buffer, .len = sizeof(m_scan_buffer)};

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;
static uint16_t m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT-3;

// Callbacks
static nrf_sdh_ble_evt_handler_t m_app_evt_cb = NULL;
static nus_rx_cb_t               m_app_nus_rec_cb = NULL;
static scan_report_cb_t          m_adv_report_cb = NULL;
static nus_c_rx_cb_t             m_nus_c_rx_cb = NULL;
static db_disc_cb_t              m_disc_cb = NULL;
static ble_db_discovery_t        m_ble_db_discovery;

// State variables
static bool        m_is_advertising = false;
static bool        m_is_central = false;
static const char *m_device_name = "";
static uint16_t    m_gatt_max_length;
static int         m_restart = 0;
static bool        m_long_range = false;
static int         m_tx_power = 0;

static void serial_init();

//--------------------------------------------------------------------------

__WEAK void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name) {
  app_error_handler(0xDEADBEEF, line_num, p_file_name);
}

//--------------------------------------------------------------------------

#if BLE_DFU_ENABLED == 1
static bool app_shutdown_handler(nrf_pwr_mgmt_evt_t event) {
  switch (event) {
    case NRF_PWR_MGMT_EVT_PREPARE_DFU:
      NRF_LOG_INFO("Power management wants to reset to DFU mode.");
      break;

    default:
      return true;
  }
  NRF_LOG_INFO("Power management allowed to reset to DFU mode.");
  return true;
}

NRF_PWR_MGMT_HANDLER_REGISTER(app_shutdown_handler, 0);

//--------------------------------------------------------------------------

static void ble_dfu_evt_handler(ble_dfu_buttonless_evt_type_t event) {
  switch (event) {
    case BLE_DFU_EVT_BOOTLOADER_ENTER_PREPARE:
      NRF_LOG_INFO("Device is preparing to enter bootloader mode.");
      break;
    case BLE_DFU_EVT_BOOTLOADER_ENTER:
      NRF_LOG_INFO("Device will enter bootloader mode.");
      break;
    case BLE_DFU_EVT_BOOTLOADER_ENTER_FAILED:
      NRF_LOG_ERROR("Request to enter bootloader mode failed asynchroneously.");
      break;
    default:
      NRF_LOG_ERROR("Unknown event from ble_dfu_buttonless.");
      break;
  }
}

//--------------------------------------------------------------------------

#endif

static void timers_init(void) {
  ret_code_t err_code = app_timer_init();
  APP_ERROR_CHECK(err_code);
}

//--------------------------------------------------------------------------

static void gap_params_init(void) {
  uint32_t err_code;
  ble_gap_conn_params_t gap_conn_params;
  ble_gap_conn_sec_mode_t sec_mode;

  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

  err_code = sd_ble_gap_device_name_set(&sec_mode,
                                        (const uint8_t *)m_device_name,
                                        strlen(m_device_name));
  APP_ERROR_CHECK(err_code);

  memset(&gap_conn_params, 0, sizeof(gap_conn_params));

  gap_conn_params.min_conn_interval = m_connection_param.min_conn_interval;
  gap_conn_params.max_conn_interval = m_connection_param.max_conn_interval;
  gap_conn_params.slave_latency = m_connection_param.slave_latency;
  gap_conn_params.conn_sup_timeout = m_connection_param.conn_sup_timeout;

  err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
  APP_ERROR_CHECK(err_code);
}

//--------------------------------------------------------------------------

static void nrf_qwr_error_handler(uint32_t nrf_error) {
  APP_ERROR_HANDLER(nrf_error);
}

//--------------------------------------------------------------------------

#define EQ_STR(s1, s2) (strcasestr(s1, s2) == s1)

static void nus_data_handler(ble_nus_evt_t *p_evt) {
  if (p_evt->type == BLE_NUS_EVT_RX_DATA) {
    if (m_app_nus_rec_cb && m_app_nus_rec_cb((uint8_t *)p_evt->params.rx_data.p_data, p_evt->params.rx_data.length))
      return;
    // Check for possible standard requests
    char str[BLE_NUS_MAX_DATA_LEN + 1];
    strlcpy(str, (const char*)p_evt->params.rx_data.p_data, MIN(sizeof(str), p_evt->params.rx_data.length+1));
    if (str[0] && str[strlen(str) - 1] == '\n')
      str[strlen(str) - 1] = 0;
    if (EQ_STR(str, "version?")) {
      char ver[80];
      snprintf(ver, sizeof(ver), "%s %s", _build_version, _build_time);
      enrf_nus_string_send(ver);
    } else if (EQ_STR(str, "mac?")) {
      enrf_nus_string_send(enrf_get_device_address());
    } else if (EQ_STR(str, "restart")) {
      m_restart = 1;
      enrf_nus_string_send("Restarting");
    } else if (EQ_STR(str, "dfu")) {
      m_restart = 2;
      enrf_nus_string_send("Entering DFU");
    } else {
      enrf_nus_string_send("* Unrecognized nus data");
    }
  }
}

//--------------------------------------------------------------------------

static void services_init(void) {
  uint32_t err_code;
  nrf_ble_qwr_init_t qwr_init = {0};
  qwr_init.error_handler = nrf_qwr_error_handler;
  err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
  APP_ERROR_CHECK(err_code);

  ble_nus_init_t nus_init;
  memset(&nus_init, 0, sizeof(nus_init));
  nus_init.data_handler = nus_data_handler;
  err_code = ble_nus_init(&m_nus, &nus_init);
  APP_ERROR_CHECK(err_code);

#if BLE_DFU_ENABLED == 1
  ble_dfu_buttonless_init_t dfus_init = {0};
  dfus_init.evt_handler = ble_dfu_evt_handler;
  err_code = ble_dfu_buttonless_init(&dfus_init);
  APP_ERROR_CHECK(err_code);
#endif
}

//--------------------------------------------------------------------------

static void on_conn_params_evt(ble_conn_params_evt_t *p_evt) {
  uint32_t err_code;
  if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED) {
    err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
    APP_ERROR_CHECK(err_code);
  }
}

//--------------------------------------------------------------------------

static void conn_params_error_handler(uint32_t nrf_error) {
  APP_ERROR_HANDLER(nrf_error);
}

//--------------------------------------------------------------------------

static void conn_params_init(void) {
  uint32_t err_code;
  ble_conn_params_init_t cp_init;
  memset(&cp_init, 0, sizeof(cp_init));
  cp_init.p_conn_params = NULL;
  cp_init.first_conn_params_update_delay = APP_TIMER_TICKS(5000);
  cp_init.next_conn_params_update_delay = APP_TIMER_TICKS(30000);
  cp_init.max_conn_params_update_count = 3;
  cp_init.start_on_notify_cccd_handle = BLE_GATT_HANDLE_INVALID;
  cp_init.disconnect_on_fail = false;
  cp_init.evt_handler = on_conn_params_evt;
  cp_init.error_handler = conn_params_error_handler;
  err_code = ble_conn_params_init(&cp_init);
  APP_ERROR_CHECK(err_code);
}

//--------------------------------------------------------------------------

static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context) {
  uint32_t err_code;

  switch (p_ble_evt->header.evt_id) {
    case BLE_GAP_EVT_CONNECTED:
      NRF_LOG_DEBUG("Connected");
      m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
      err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
      APP_ERROR_CHECK(err_code);
      if (m_is_central) {
        memset(&m_ble_db_discovery, 0, sizeof(m_ble_db_discovery));
        ble_db_discovery_start(&m_ble_db_discovery, p_ble_evt->evt.gap_evt.conn_handle);
      }
      break;

    case BLE_GAP_EVT_DISCONNECTED:
      NRF_LOG_DEBUG("Disconnected: reason 0x%x.", p_ble_evt->evt.gap_evt.params.disconnected.reason);
      m_conn_handle = BLE_CONN_HANDLE_INVALID;
      if (m_is_advertising)
        sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
      m_is_central = false;
      break;

    case BLE_GAP_EVT_PHY_UPDATE_REQUEST: {
      NRF_LOG_DEBUG("PHY update request.");
      ble_gap_phys_t const phys =
      {
        .rx_phys = BLE_GAP_PHY_AUTO,
        .tx_phys = BLE_GAP_PHY_AUTO,
      };
      err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
      APP_ERROR_CHECK(err_code);
    } break;

    case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
      // Pairing not supported
      err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
      APP_ERROR_CHECK(err_code);
      break;

    case BLE_GAP_EVT_ADV_REPORT: {
      ble_evt_t *p = (ble_evt_t *)p_ble_evt;
      ble_gap_evt_t *p_gap_evt = &p->evt.gap_evt;
      if (!m_adv_report_cb || !m_adv_report_cb(&p_gap_evt->params.adv_report))
        if (sd_ble_gap_scan_start(NULL, &m_adv_rep_buffer) != NRF_SUCCESS)
          NRF_LOG_ERROR("Failed to restart scanning");
      break;
    }

    case BLE_GAP_EVT_TIMEOUT: {
      const ble_gap_evt_t *p_gap_evt = &p_ble_evt->evt.gap_evt;
      if (p_gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_SCAN) {
        NRF_LOG_ERROR("Scan timed out");
      } else if (p_gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN) {
        NRF_LOG_ERROR("Connection Request timed out");
      }
      break;
    }

    case BLE_GATTS_EVT_SYS_ATTR_MISSING:
      // No system attributes have been stored.
      err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
      APP_ERROR_CHECK(err_code);
      break;

    case BLE_GATTC_EVT_TIMEOUT:
      // Disconnect on GATT Client timeout event.
      err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                       BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
      APP_ERROR_CHECK(err_code);
      break;

    case BLE_GATTS_EVT_TIMEOUT:
      // Disconnect on GATT Server timeout event.
      err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                       BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
      APP_ERROR_CHECK(err_code);
      break;

    case BLE_GAP_EVT_DATA_LENGTH_UPDATE:
      m_gatt_max_length = p_ble_evt->evt.gap_evt.params.data_length_update.effective_params.max_tx_octets;
      break;

    default:
      break;
  }

  if (m_is_central)
    ble_db_discovery_on_ble_evt(p_ble_evt, &m_ble_db_discovery);

  if (m_app_evt_cb)
    m_app_evt_cb(p_ble_evt, p_context);
}

//--------------------------------------------------------------------------

static void ble_stack_init(void) {
  ret_code_t err_code;

  err_code = nrf_sdh_enable_request();
  APP_ERROR_CHECK(err_code);

  // Configure the BLE stack using the default settings.
  // Fetch the start address of the application RAM.
  uint32_t ram_start = 0;
  err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
  APP_ERROR_CHECK(err_code);

  // Enable BLE stack.
  err_code = nrf_sdh_ble_enable(&ram_start);
  APP_ERROR_CHECK(err_code);

  // Register a handler for BLE events.
  NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}

//--------------------------------------------------------------------------

static void gatt_evt_handler(nrf_ble_gatt_t *p_gatt, nrf_ble_gatt_evt_t const *p_evt) {
  if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED)) {
    m_ble_nus_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
    NRF_LOG_DEBUG("Data len is set to 0x%X(%d)", m_ble_nus_max_data_len, m_ble_nus_max_data_len);
  }
  NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x",
                p_gatt->att_mtu_desired_central,
                p_gatt->att_mtu_desired_periph);
}

//--------------------------------------------------------------------------

static void gatt_init(void) {
  ret_code_t err_code;
  err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
  APP_ERROR_CHECK(err_code);
  err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
  APP_ERROR_CHECK(err_code);
}

//--------------------------------------------------------------------------

static void log_init(void) {
  ret_code_t err_code = NRF_LOG_INIT(NULL);
  APP_ERROR_CHECK(err_code);
  NRF_LOG_DEFAULT_BACKENDS_INIT();
}

//--------------------------------------------------------------------------

static void power_management_init(void) {
  ret_code_t err_code;
  err_code = nrf_pwr_mgmt_init();
  APP_ERROR_CHECK(err_code);
}

//--------------------------------------------------------------------------

static void db_disc_handler(ble_db_discovery_evt_t *p_evt) {
  if (m_nus_c_rx_cb)
    ble_nus_c_on_db_disc_evt(&m_ble_nus_c, p_evt);
  if (m_disc_cb)
    m_disc_cb(p_evt);
}

//--------------------------------------------------------------------------

bool enrf_init(const char *dev_name, nrf_sdh_ble_evt_handler_t ble_evt_cb) {
  ret_code_t err_code;
  serial_init();
  m_device_name = dev_name;
  log_init();
#if BLE_DFU_ENABLED == 1
  err_code = ble_dfu_buttonless_async_svci_init();
  APP_ERROR_CHECK(err_code);
#endif
  timers_init();
  power_management_init();
  ble_stack_init();
  gap_params_init();
  gatt_init();
  services_init();
  conn_params_init();
#if SDK_VERSION >= 17
  ble_db_discovery_init_t db_init;
  memset(&db_init, 0, sizeof(ble_db_discovery_init_t));
  db_init.evt_handler = db_disc_handler;
  db_init.p_gatt_queue = &m_ble_gatt_queue;
  err_code = ble_db_discovery_init(&db_init);
#else
  err_code = ble_db_discovery_init(db_disc_handler);
#endif
  APP_ERROR_CHECK(err_code);

  m_app_evt_cb = ble_evt_cb;

  NRF_LOG_INFO("%s, version: %s %s", dev_name, _build_version, _build_time);
  NRF_LOG_INFO("Mac address: %s", enrf_get_device_address());

  return true;
}

//--------------------------------------------------------------------------

void enrf_set_phy(bool long_range) {
  m_long_range = long_range;
}

//--------------------------------------------------------------------------

void enrf_set_tx_power(int tx_power) {
  m_tx_power = MAX(-40, MIN(8, tx_power));
}

//--------------------------------------------------------------------------

ret_code_t enrf_start_advertise(bool connectable,
                                uint16_t company_id, ble_advdata_name_type_t type,
                                uint8_t *p_data, uint8_t size,
                                uint32_t interval_ms, uint32_t timeout_s,
                                nus_rx_cb_t nus_cb) {
  uint32_t err_code;
  ble_advdata_t advdata;

  enrf_stop_advertise(m_adv_handle);

  m_app_nus_rec_cb = nus_cb;

  // Set advertisement data
  memset(&advdata, 0, sizeof(advdata));
  advdata.name_type = type;
  advdata.flags = connectable ? BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE : BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;
  if (p_data != NULL) {
    static ble_advdata_manuf_data_t manuf_specific_data;
    manuf_specific_data.company_identifier = company_id;
    manuf_specific_data.data.p_data = p_data;
    manuf_specific_data.data.size = size;
    advdata.p_manuf_specific_data = &manuf_specific_data;
  }

  m_adv_data.adv_data.len = sizeof(m_enc_advdata);

  // Set advertisement parameters
  memset(&m_adv_params, 0, sizeof(m_adv_params));
  if (m_long_range) {
    m_adv_params.properties.type =
      connectable ? BLE_GAP_ADV_TYPE_EXTENDED_CONNECTABLE_NONSCANNABLE_UNDIRECTED : BLE_GAP_ADV_TYPE_EXTENDED_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED;
    m_adv_params.primary_phy = BLE_GAP_PHY_CODED;
    m_adv_params.secondary_phy = BLE_GAP_PHY_CODED;
  } else {
    m_adv_params.properties.type =
        connectable ? BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED : BLE_GAP_ADV_TYPE_NONCONNECTABLE_SCANNABLE_UNDIRECTED;
    m_adv_params.primary_phy = BLE_GAP_PHY_1MBPS;
    m_adv_params.secondary_phy = BLE_GAP_PHY_1MBPS;
  }
  m_adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
  m_adv_params.interval = MSEC_TO_UNITS(interval_ms, UNIT_0_625_MS);
  m_adv_params.duration = timeout_s * 100;
  err_code = ble_advdata_encode(&advdata, m_adv_data.adv_data.p_data, &m_adv_data.adv_data.len);
  APP_ERROR_CHECK(err_code);
  err_code = sd_ble_gap_adv_set_configure(&m_adv_handle, &m_adv_data, &m_adv_params);
  APP_ERROR_CHECK(err_code);

  // And transmission output power
  err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV, m_adv_handle, m_tx_power);
  APP_ERROR_CHECK(err_code);

  m_is_central = false;

  NRF_LOG_DEBUG("Advertising set, lr: %d, tx: %d dBm", m_long_range, m_tx_power);
  err_code = sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
  m_is_advertising = err_code == NRF_SUCCESS;

  return err_code;
}

//--------------------------------------------------------------------------

ret_code_t enrf_stop_advertise() {
  m_is_advertising = false;
  return sd_ble_gap_adv_stop(m_adv_handle);
}

//--------------------------------------------------------------------------

bool enrf_is_connected() {
  return m_conn_handle != BLE_CONN_HANDLE_INVALID;
}

//--------------------------------------------------------------------------

ret_code_t enrf_nus_data_send(const uint8_t *data, uint32_t length) {
  ret_code_t err_code = NRF_SUCCESS;
  while (length) {
    uint16_t curr = m_gatt_max_length < 30 ? MIN(20, length) : length;
    do {
      err_code = ble_nus_data_send(&m_nus, (uint8_t*)data, &curr, m_conn_handle);
    } while (err_code == NRF_ERROR_RESOURCES);
    length -= curr;
    data += curr;
  }
  return err_code;
}

//--------------------------------------------------------------------------

ret_code_t enrf_nus_string_send(const char *str) {
  return enrf_nus_data_send((uint8_t *)str, strlen(str) + 1);
}

//--------------------------------------------------------------------------

void enrf_set_scan_par(uint16_t scan_int, uint16_t scan_wind) {
  m_scan_params.interval = scan_int;
  m_scan_params.window = scan_wind;
}

//--------------------------------------------------------------------------

ret_code_t enrf_start_scan(scan_report_cb_t report_cb, uint32_t timeout_s, bool active) {
  sd_ble_gap_scan_stop();
  m_scan_params.active = active ? 1 : 0;
  m_scan_params.extended = m_long_range ? 1 : 0;
  m_scan_params.timeout = timeout_s*100;
  m_scan_params.scan_phys = m_long_range ? BLE_GAP_PHY_CODED : BLE_GAP_PHY_AUTO;
  m_scan_params.filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL;
  sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_SCAN_INIT, 0, m_tx_power);
  m_adv_report_cb = report_cb;
  return sd_ble_gap_scan_start(&m_scan_params, &m_adv_rep_buffer);
}

//--------------------------------------------------------------------------

ret_code_t enrf_stop_scan() {
  return sd_ble_gap_scan_stop();
}

//--------------------------------------------------------------------------

uint8_t enrf_adv_parse(ble_gap_evt_adv_report_t *p_adv_report,
                       uint8_t start_tag, uint8_t end_tag,
                       uint8_t *dest, uint8_t dest_len) {
  size_t offset;
  offset = 0;
  while (offset < p_adv_report->data.len) {
    uint8_t field_len = p_adv_report->data.p_data[offset];
    uint8_t field_type = p_adv_report->data.p_data[offset + 1];

    if (offset + field_len > p_adv_report->data.len)
      return 0;

    if (field_type >= start_tag && field_type <= end_tag) {
      uint8_t len = field_len - 1;
      if (len > dest_len)
        return 0;
      memcpy(dest, &(p_adv_report->data.p_data[offset + 2]), len);
      return len;
    }
    offset += field_len + 1;
  }
  return 0;
}

//--------------------------------------------------------------------------

ret_code_t enrf_disconnect() {
  return sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
}

//--------------------------------------------------------------------------

void enrf_set_connection_params(float min_con_int_ms, float max_con_int_ms, uint16_t slave_latency, float sup_timeout_ms) {
  m_connection_param.min_conn_interval = MSEC_TO_UNITS(min_con_int_ms, UNIT_1_25_MS);
  m_connection_param.max_conn_interval = MSEC_TO_UNITS(max_con_int_ms, UNIT_1_25_MS);
  m_connection_param.slave_latency = slave_latency;
  m_connection_param.conn_sup_timeout = MSEC_TO_UNITS(sup_timeout_ms, UNIT_10_MS);
}

//--------------------------------------------------------------------------

static void ble_nus_c_evt_handler(ble_nus_c_t *p_ble_nus_c, const ble_nus_c_evt_t *p_ble_nus_evt) {
  uint32_t err_code;
  switch (p_ble_nus_evt->evt_type) {
    case BLE_NUS_C_EVT_DISCOVERY_COMPLETE:
      // UART service detected
      err_code = ble_nus_c_handles_assign(p_ble_nus_c, p_ble_nus_evt->conn_handle, &p_ble_nus_evt->handles);
      APP_ERROR_CHECK(err_code);
      err_code = ble_nus_c_tx_notif_enable(p_ble_nus_c);
      APP_ERROR_CHECK(err_code);
      if (m_nus_c_rx_cb)
        m_nus_c_rx_cb(NULL, 1);
      break;

    case BLE_NUS_C_EVT_NUS_TX_EVT:
      // UART response received
      if (m_nus_c_rx_cb)
        m_nus_c_rx_cb(p_ble_nus_evt->p_data, p_ble_nus_evt->data_len);
      break;

    case BLE_NUS_C_EVT_DISCONNECTED:
      if (m_nus_c_rx_cb)
        m_nus_c_rx_cb(NULL, 0);
      break;
  }
}

//--------------------------------------------------------------------------

ret_code_t enrf_connect_to(ble_gap_addr_t *addr, db_disc_cb_t disc_cb, nus_c_rx_cb_t nus_c_rx_cb) {
  static bool nus_init = false;
  m_disc_cb = disc_cb;
  m_nus_c_rx_cb = nus_c_rx_cb;
  if (nus_c_rx_cb && !nus_init) {
    ble_nus_c_init_t nus_c_init_t;
    nus_c_init_t.evt_handler = ble_nus_c_evt_handler;
    APP_ERROR_CHECK(ble_nus_c_init(&m_ble_nus_c, &nus_c_init_t));
    nus_init = true;
  }
  m_is_central = true;
  return sd_ble_gap_connect(addr, &m_scan_params, &m_connection_param, APP_BLE_CONN_CFG_TAG);
}

//--------------------------------------------------------------------------

ret_code_t enrf_enable_char_notif(uint16_t cccd_handle, bool enable) {
  uint8_t buf[BLE_CCCD_VALUE_LEN];
  buf[0] = enable ? BLE_GATT_HVX_NOTIFICATION : 0;
  buf[1] = 0;
  const ble_gattc_write_params_t write_params = {
      .write_op = BLE_GATT_OP_WRITE_REQ,
      .flags = BLE_GATT_EXEC_WRITE_FLAG_PREPARED_WRITE,
      .handle = cccd_handle,
      .offset = 0,
      .len = sizeof(buf),
      .p_value = buf};
  return sd_ble_gattc_write(m_conn_handle, &write_params);
}

//--------------------------------------------------------------------------

ret_code_t enrf_write_char(uint8_t op, uint16_t char_handle, uint8_t *data, uint16_t length) {
  const ble_gattc_write_params_t write_params = {
      .write_op = op,
      .flags = BLE_GATT_EXEC_WRITE_FLAG_PREPARED_WRITE,
      .handle = char_handle,
      .offset = 0,
      .len = length,
      .p_value = data};
  return sd_ble_gattc_write(m_conn_handle, &write_params);
}

//--------------------------------------------------------------------------

ret_code_t enrf_read_char(uint16_t char_handle) {
  return sd_ble_gattc_read(m_conn_handle, char_handle, 0);
}

//--------------------------------------------------------------------------

ret_code_t enrf_nus_c_data_send(const uint8_t *data, uint32_t length) {
  return ble_nus_c_string_send(&m_ble_nus_c, (uint8_t *)data, length);
}

//--------------------------------------------------------------------------

ret_code_t enrf_nus_c_string_send(const char *str) {
  return enrf_nus_c_data_send((const uint8_t *)str, strlen(str));
}

//--------------------------------------------------------------------------

void enrf_wait_for_event() {
#ifdef ENRF_SERIAL_USB
  while (app_usbd_event_queue_process());
#endif
  if (m_restart)
    enrf_restart(m_restart == 2);
  if (NRF_LOG_PROCESS() == false) {
    nrf_pwr_mgmt_run();
  }
}

//--------------------------------------------------------------------------

void enrf_restart(bool enter_dfu) {
  NRF_LOG_INFO("Restarting: %d", enter_dfu);
  if (enter_dfu) {
    sd_power_gpregret_clr(0, 0xffffffff);
    sd_power_gpregret_set(0, 0xB1);
  }
  if (m_conn_handle != BLE_CONN_HANDLE_INVALID) {
    sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
  }
  NRF_LOG_PROCESS();
  nrf_delay_ms(1000);
  NVIC_SystemReset();
}

//--------------------------------------------------------------------------

ret_code_t enrf_add_uuid(const char *uuid) {
  ret_code_t res;
  ble_uuid128_t base_uuid;
  ble_uuid_t service_uuid;
  const char *s = uuid;
  int ind = 15;
  bool ok = true;
  // Read base uuid hex values, big endian
  while (ok && *s != 0 && *s != ',') {
    if (*s == '-') {
      // Uuid field separator
      s++;
      continue;
    }
    unsigned int b;
    ok = sscanf(s, "%02X", &b) == 1;
    *(base_uuid.uuid128 + (ind--)) = b;
    s += 2;
  }
  ok = ok && ind == -1 && s != NULL;
  if (ok) {
    res = sd_ble_uuid_vs_add(&base_uuid, &service_uuid.type);
    if (res == NRF_SUCCESS) {
      // Service uuid
      service_uuid.uuid = *((uint16_t *)&base_uuid.uuid128 + 6);
      if (service_uuid.uuid)
        res = ble_db_discovery_evt_register(&service_uuid);
    }
  } else {
    res = NRF_ERROR_INVALID_PARAM;
    NRF_LOG_ERROR("Invalid UUID");
  }
  return res;
}

//--------------------------------------------------------------------------

const char *enrf_addr_to_str(ble_gap_addr_t *gap_addr) {
  static char addr[18];
  addr[0] = 0;
  snprintf(addr, sizeof(addr),
           "%02X:%02X:%02X:%02X:%02X:%02X",
           gap_addr->addr[5],
           gap_addr->addr[4],
           gap_addr->addr[3],
           gap_addr->addr[2],
           gap_addr->addr[1],
           gap_addr->addr[0]);
  return addr;
}

//--------------------------------------------------------------------------

bool enrf_str_to_addr(const char *addr_str, ble_gap_addr_t *gap_addr) {
  int x[6];
  int cnt = sscanf(addr_str, "%X:%X:%X:%X:%X:%X", &x[5], &x[4], &x[3], &x[2], &x[1], &x[0]);
  for (int i = 0; i < cnt; i++)
    gap_addr->addr[i] = x[i];
  return cnt == 6;
}

//--------------------------------------------------------------------------

const char *enrf_get_device_address() {
  ble_gap_addr_t ble_gap_addr;
  if (sd_ble_gap_addr_get(&ble_gap_addr) == NRF_SUCCESS)
    return enrf_addr_to_str(&ble_gap_addr);
  else
    return "?";
}

//--------------------------------------------------------------------------

static inline uint8_t _hex_val(char val) {
  if (val >= '0' && val <= '9')
    return (uint8_t)(val) - '0';
  else if (val >= 'A' && val <= 'F')
    return (uint8_t)(val) - 'A' + 10;
  else if (val >= 'a' && val <= 'f')
    return (uint8_t)(val) - 'a' + 10;
  else
    return 0xFF;
}

//--------------------------------------------------------------------------

uint32_t hex_to_bytes(const char *str, uint8_t *bytes, uint32_t max_len) {
  uint32_t len = MIN(strlen(str) / 2, max_len);
  for (uint32_t i = 0; i < len; i++) {
    uint8_t val = _hex_val(*str++);
    if (val > 15) return 0;
    *bytes = val << 4;
    val = _hex_val(*str++);
    if (val > 15) return 0;
    *bytes += val;
    bytes++;
  }
  return len;
}

//--------------------------------------------------------------------------

static inline char _hex_char(uint8_t val) {
  if (val >= 0 && val <= 9)
    return '0' + val;
  else if (val >= 10 && val <= 15)
    return 'A' + val - 10;
  return 0;
}

//--------------------------------------------------------------------------

void bytes_to_hex(uint8_t *bytes, uint32_t len, char *str) {
  for (uint32_t i = 0; i < len; i++) {
    uint8_t val = bytes[i];
    *(str++) = _hex_char(val >> 4);
    *(str++) = _hex_char(val & 0xF);
  }
  *str = 0;
}

//--------------------------------------------------------------------------

#if defined(ENRF_SERIAL_USB) || defined(ENRF_SERIAL_UART)

#define READ_BUFF_SIZE 100

static bool m_input_available = false;
static size_t m_input_pos = 0;
static char m_input_buffer[READ_BUFF_SIZE];

size_t enrf_serial_read(char *str, size_t max_length) {
  if (!m_input_available) return 0;
  size_t len = MIN(max_length - 1, m_input_pos);
  strncpy(str, m_input_buffer, len);
  str[len] = 0;
  m_input_pos = 0;
  m_input_available = false;
  return len;
}

#endif

#ifdef ENRF_SERIAL_USB

#define CDC_ACM_COMM_INTERFACE 0
#define CDC_ACM_COMM_EPIN NRF_DRV_USBD_EPIN2

#define CDC_ACM_DATA_INTERFACE 1
#define CDC_ACM_DATA_EPIN NRF_DRV_USBD_EPIN1
#define CDC_ACM_DATA_EPOUT NRF_DRV_USBD_EPOUT1

#define READ_SIZE 1

static void cdc_acm_user_ev_handler(app_usbd_class_inst_t const *p_inst,
                                    app_usbd_cdc_acm_user_event_t event);
static void usbd_user_ev_handler(app_usbd_event_type_t event);

APP_USBD_CDC_ACM_GLOBAL_DEF(m_app_cdc_acm,
                            cdc_acm_user_ev_handler,
                            CDC_ACM_COMM_INTERFACE,
                            CDC_ACM_DATA_INTERFACE,
                            CDC_ACM_COMM_EPIN,
                            CDC_ACM_DATA_EPIN,
                            CDC_ACM_DATA_EPOUT,
                            APP_USBD_CDC_COMM_PROTOCOL_NONE);

static const app_usbd_config_t m_usbd_config = {.ev_state_proc = usbd_user_ev_handler};

static void cdc_acm_user_ev_handler(app_usbd_class_inst_t const *p_inst,
                                    app_usbd_cdc_acm_user_event_t event) {
  static char rx_buffer[READ_SIZE];

  switch (event) {
    case APP_USBD_CDC_ACM_USER_EVT_PORT_OPEN: {
      // Setup first transfer
      app_usbd_cdc_acm_read(&m_app_cdc_acm, rx_buffer, READ_SIZE);
      break;
    }
    case APP_USBD_CDC_ACM_USER_EVT_PORT_CLOSE:
      break;
    case APP_USBD_CDC_ACM_USER_EVT_TX_DONE:
      break;
    case APP_USBD_CDC_ACM_USER_EVT_RX_DONE: {
      ret_code_t ret;
      do {
        if (!m_input_available && rx_buffer[0] != '\r') {
          if (rx_buffer[0] == '\n' || m_input_pos >= READ_BUFF_SIZE)
            m_input_available = true;
          else
            m_input_buffer[m_input_pos++] = rx_buffer[0];
        }
        // Fetch data until internal buffer is empty
        ret = app_usbd_cdc_acm_read(&m_app_cdc_acm, rx_buffer, READ_SIZE);
      } while (ret == NRF_SUCCESS);

      break;
    }
    default:
      break;
  }
}

//--------------------------------------------------------------------------

static void usbd_user_ev_handler(app_usbd_event_type_t event) {
  switch (event) {
    case APP_USBD_EVT_DRV_SUSPEND:
      break;
    case APP_USBD_EVT_DRV_RESUME:
      break;
    case APP_USBD_EVT_STARTED:
      break;
    case APP_USBD_EVT_STOPPED:
      app_usbd_disable();
      bsp_board_leds_off();
      break;
    case APP_USBD_EVT_POWER_DETECTED:
      NRF_LOG_INFO("USB power detected");
      if (!nrf_drv_usbd_is_enabled()) {
        app_usbd_enable();
      }
      break;
    case APP_USBD_EVT_POWER_REMOVED:
      NRF_LOG_INFO("USB power removed");
      app_usbd_stop();
      break;
    case APP_USBD_EVT_POWER_READY:
      NRF_LOG_INFO("USB ready");
      app_usbd_start();
      break;
    default:
      break;
  }
}

//--------------------------------------------------------------------------

void serial_init() {
  ret_code_t ret;
  app_usbd_serial_num_generate();

  ret = app_usbd_init(&m_usbd_config);
  APP_ERROR_CHECK(ret);
  app_usbd_class_inst_t const *class_cdc_acm = app_usbd_cdc_acm_class_inst_get(&m_app_cdc_acm);
  ret = app_usbd_class_append(class_cdc_acm);
  APP_ERROR_CHECK(ret);

  ret = app_usbd_power_events_enable();
  APP_ERROR_CHECK(ret);
}

//--------------------------------------------------------------------------

ret_code_t enrf_serial_write(const char *str) {
  ret_code_t res = NRF_ERROR_BUSY;
  static char buff[255];
  strlcpy(buff, str, sizeof(buff));
  do {
    res = app_usbd_cdc_acm_write(&m_app_cdc_acm, buff, strlen(buff));
    app_usbd_event_queue_process();
  } while (res == NRF_ERROR_BUSY);
  return res;
}

//--------------------------------------------------------------------------


#elif defined(ENRF_SERIAL_UART)

#define UART_TX_BUF_SIZE 256
#define UART_RX_BUF_SIZE 256

static void uart_event_handler(app_uart_evt_t *p_event) {
  uint8_t ch;
  switch (p_event->evt_type) {
    case APP_UART_DATA_READY:
      app_uart_get(&ch);
      if (!m_input_available && ch != '\r') {
        if (ch == '\n' || m_input_pos >= READ_BUFF_SIZE)
          m_input_available = true;
        else
          m_input_buffer[m_input_pos++] = ch;
      }
      break;

    default:
      break;
  }
}

//--------------------------------------------------------------------------

void serial_init() {
  uint32_t err_code;
  app_uart_comm_params_t comm_params;
  comm_params.rx_pin_no = RX_PIN_NUMBER;
  comm_params.tx_pin_no = TX_PIN_NUMBER;
  comm_params.flow_control = APP_UART_FLOW_CONTROL_DISABLED;
  comm_params.use_parity = false;
  comm_params.baud_rate = UART_BAUD_RATE;

  APP_UART_FIFO_INIT(&comm_params,
                      UART_RX_BUF_SIZE,
                      UART_TX_BUF_SIZE,
                      uart_event_handler,
                      APP_IRQ_PRIORITY_LOWEST,
                      err_code);
  APP_ERROR_CHECK(err_code);
}

//--------------------------------------------------------------------------

ret_code_t enrf_serial_write(const char *str) {
  ret_code_t err_code = NRF_SUCCESS;
  for (size_t i = 0; i < strlen(str) && err_code == NRF_SUCCESS; i++)
    err_code = app_uart_put(str[i]);

  return err_code;
}

//--------------------------------------------------------------------------


#else

void serial_init() {
}

//--------------------------------------------------------------------------

ret_code_t enrf_serial_write(const char *str) {
  return NRF_ERROR_API_NOT_IMPLEMENTED;
}

//--------------------------------------------------------------------------

size_t enrf_serial_read(char *str, size_t max_length) {
  return 0;
}

#endif