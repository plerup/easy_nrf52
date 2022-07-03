//====================================================================================
//
// ble_tool
//
// This application can perform various BLE operations
// using the enrf functions
// It is controlled by commands from uart or usb and can act
// as central, peripheral or both
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
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>

#define BUFF_SIZE 255
#define MAX_PARAMS 6

// Response output macros
#define RESP(_form, ...) format_response(_form, ##__VA_ARGS__)
#define RESP_ASYNC(form, ...) RESP("#" form, ##__VA_ARGS__)
#define RESP_ERROR(form, ...) RESP("*" form, ##__VA_ARGS__)
#define RESP_OK(form, ...)    RESP("=" form, ##__VA_ARGS__)

// Command input and output macros
#define CMD_OK(form, ...) RESP_OK("%s "form, m_command, ##__VA_ARGS__)
#define CMD_ERROR(mess) RESP_ERROR("%s %s", m_command, mess)
#define BOOL_PARAM(pos) (m_params[pos] != NULL && *m_params[pos] == '1')
#define DEC_PARAM(pos, def) (m_params[pos] ? strtoul(m_params[pos], NULL, 10) : def)
#define VALIDATE_NRF(check) do { ret_code_t r = check; if (r == NRF_SUCCESS) { CMD_OK(); } else RESP_ERROR("%s nrf error: %lX", m_command, r); } while (0)

// Temporary buffers
char m_char_buff[BUFF_SIZE];
uint8_t m_data_buff[BUFF_SIZE];

// Scan matching string
char m_scan_match[100] = {0};
bool m_scan_once;

// Current received command
char m_command[BUFF_SIZE];
int m_param_cnt = 0;
const char *m_params[MAX_PARAMS];

static char *hex_str(uint8_t *data, uint16_t len) {
  bytes_to_hex(data, MIN(len, sizeof(m_char_buff) / 2 - 1), m_char_buff);
  return m_char_buff;
}

//--------------------------------------------------------------------------

static void format_response(const char *form, ...) {
  char buff[255];
  va_list args;
  va_start(args, form);
  vsnprintf(buff, sizeof(buff), form, args);
  va_end(args);
  strlcat(buff, "\n", sizeof(buff));
  enrf_serial_write(buff);
}

//--------------------------------------------------------------------------

static bool nus_data_received(uint8_t *data, uint32_t length) {
  strlcpy(m_char_buff, (const char*)data, MIN(sizeof(m_char_buff), length));
  char *end_pos = m_char_buff + strlen(m_char_buff) - 1;
  // Trim possible trailing newline
  if (*end_pos == '\n')
    *end_pos = 0;
  RESP_ASYNC("NUS:%s", m_char_buff);
  return false;
}

//--------------------------------------------------------------------------

void nus_c_response(uint8_t *data, uint32_t length) {
  if (!data && length) {
    RESP_ASYNC("NUS_DETECTED");
  } else if (data) {
    RESP_ASYNC("NUSC:%s", (char*)data);
  }
}

//--------------------------------------------------------------------------

static void on_ble_evt(const ble_evt_t *p_ble_evt, void *p_context) {
  switch (p_ble_evt->header.evt_id) {
    case BLE_GAP_EVT_CONNECTED:
      RESP_ASYNC("CONNECTED");
      break;

    case BLE_GAP_EVT_DISCONNECTED:
      RESP_ASYNC("DISCONNECTED 0x%x", p_ble_evt->evt.gap_evt.params.disconnected.reason);
      break;

    case BLE_GATTC_EVT_WRITE_RSP:
      RESP_ASYNC("WRITE_RESP:%X,%s", p_ble_evt->evt.gattc_evt.params.write_rsp.handle,
                 hex_str((uint8_t *)p_ble_evt->evt.gattc_evt.params.write_rsp.data, p_ble_evt->evt.gattc_evt.params.write_rsp.len));
      break;

    case BLE_GATTC_EVT_READ_RSP:
      RESP_ASYNC("READ_RESP:%X,%s", p_ble_evt->evt.gattc_evt.params.read_rsp.handle,
                 hex_str((uint8_t *)p_ble_evt->evt.gattc_evt.params.read_rsp.data, p_ble_evt->evt.gattc_evt.params.read_rsp.len));
      break;

    case BLE_GATTC_EVT_HVX:
      RESP_ASYNC("NOTIF:%X,%s", p_ble_evt->evt.gattc_evt.params.hvx.handle,
                 hex_str((uint8_t *)p_ble_evt->evt.gattc_evt.params.hvx.data, p_ble_evt->evt.gattc_evt.params.hvx.len));
      break;

    case BLE_GAP_EVT_TIMEOUT: {
      const ble_gap_evt_t *p_gap_evt = &p_ble_evt->evt.gap_evt;
      if (p_gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_SCAN) {
        RESP_ASYNC("SCAN:Time out");
      } else if (p_gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN) {
        RESP_ASYNC("CONNECT:Time out");
      }
      break;
    }
    case BLE_GAP_EVT_ADV_SET_TERMINATED:
      RESP_ASYNC("ADVERTISE:Time out");
      break;
    default:
      break;
  }
}

//--------------------------------------------------------------------------

static bool scan_response(ble_gap_evt_adv_report_t *p_adv_report) {
  // Build scan report
  uint8_t name[32];
  uint8_t name_len = enrf_adv_parse(p_adv_report, BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, name, sizeof(name));
  name[name_len] = 0;
  snprintf(m_char_buff, sizeof(m_char_buff), "%s;%s;", enrf_addr_to_str(&(p_adv_report->peer_addr)), name);
  uint8_t data[32];
  uint8_t data_len = enrf_adv_parse(p_adv_report, BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA, BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA, data, sizeof(data));
  bytes_to_hex(data, data_len, m_char_buff + strlen(m_char_buff));
  // Check for match
  char *match_pos = m_scan_match;
  bool show = *match_pos == 0;  // Empty match means all
  while (!show && *match_pos) {
    // Search for possible match in the list
    show = strstr(m_char_buff, match_pos) != NULL;
    match_pos += strlen(match_pos) + 1;
  }
  if (show) {
    RESP_ASYNC("SCAN:%s;%d", (char *)m_char_buff, p_adv_report->rssi);
  }
  return m_scan_once && show;
}

//--------------------------------------------------------------------------

void discovery_handler(ble_db_discovery_evt_t *p_evt) {
  ble_gatt_db_char_t *p_chars = p_evt->params.discovered_db.charateristics;
  if (p_evt->evt_type == BLE_DB_DISCOVERY_COMPLETE) {
    // Show handle-uuid mapping of all discovered known services
    for (int i = 0; i < p_evt->params.discovered_db.char_count; i++) {
      RESP_ASYNC("DISC_HANDLE:%X,%X,%X,%X",
                 p_evt->params.discovered_db.srv_uuid.uuid,
                 p_chars[i].characteristic.uuid.uuid,
                 p_chars[i].characteristic.handle_value,
                 p_chars[i].cccd_handle);
    }
    RESP_ASYNC("DISC_DONE");
  }
}

//--------------------------------------------------------------------------

static void scan() {
  // Parameter format: match_string;only_once;long_range;active;timeout
  if (!m_param_cnt) {
    VALIDATE_NRF(enrf_stop_scan());
  } else {
    // Set up match string
    memset(m_scan_match, 0, sizeof(m_scan_match));
    strlcpy(m_scan_match, m_params[0], sizeof(m_scan_match) - 2);
    char *pos = m_scan_match;
    // Split the match on possible separators
    while ((pos = strchr(pos, '|')) != NULL) {
      *(pos++) = 0;
    }
    m_scan_once = BOOL_PARAM(1);
    enrf_set_phy(BOOL_PARAM(2));
    VALIDATE_NRF(enrf_start_scan(scan_response, DEC_PARAM(4, 0), BOOL_PARAM(3)));
  }
}

//--------------------------------------------------------------------------

static void advertise() {
  // Parameter format: name|manuf_data;connectable;long_range;timeout_s;interval_ms
  if (!m_param_cnt) {
    VALIDATE_NRF(enrf_stop_advertise());
  } else {
    uint32_t timeout_s = DEC_PARAM(3, 0);
    uint32_t interval_ms = DEC_PARAM(4, 100);
    enrf_set_phy(BOOL_PARAM(2));
    if (*m_params[0] == '#') {
      // Manufacturer data field
      uint8_t data[32];
      uint32_t len = hex_to_bytes(m_params[0]+1, data, sizeof(data));
      VALIDATE_NRF(enrf_start_advertise(BOOL_PARAM(1), *((uint16_t *)data), BLE_ADVDATA_NO_NAME,
                                        data + 2, len - 2,
                                        interval_ms, timeout_s, nus_data_received));
    } else {
      // Name field
      ble_gap_conn_sec_mode_t sec_mode;
      BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
      sd_ble_gap_device_name_set(&sec_mode, (const uint8_t*)m_params[0], strlen(m_params[0]));
      VALIDATE_NRF(enrf_start_advertise(BOOL_PARAM(1), 0, BLE_ADVDATA_FULL_NAME,
                                       NULL, 0,
                                       interval_ms, timeout_s, nus_data_received));
    }
  }
}

//--------------------------------------------------------------------------

static void add_uuid() {
  ble_uuid128_t base_uuid;
  ble_uuid_t service_uuid;
  const char *s = m_params[0];
  int ind = 15;
  bool ok = true;
  // Read base uuid hex values, big endian
  while (ok && *s != 0 && *s != ',') {
    if (*s == '-') {
      // Uuid field separator
      s++;
      continue;
    }
    ok = hex_to_bytes(s, base_uuid.uuid128 + (ind--), 1) == 1;
    s += 2;
  }
  ok = ok && ind == -1 && s != NULL;
  if (ok) {
    ret_code_t res = sd_ble_uuid_vs_add(&base_uuid, &service_uuid.type);
    if (res != NRF_SUCCESS) {
      VALIDATE_NRF(res);
    } else {
      // Service uuid
      service_uuid.uuid = *((uint16_t *)&base_uuid.uuid128 + 6);
      if (service_uuid.uuid)
        VALIDATE_NRF(ble_db_discovery_evt_register(&service_uuid));
      else
        CMD_OK();
    }
  } else {
    CMD_ERROR("Invalid UUID");
  }
}

//--------------------------------------------------------------------------

static void connect() {
  // Parameter format: mac_address;long_range;
  ble_gap_addr_t addr = {0};
  if (*m_params[0] == 'P') {
    m_params[0]++;
    addr.addr_type = BLE_GAP_ADDR_TYPE_PUBLIC;
  } else {
    addr.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
  }
  if (enrf_str_to_addr(m_params[0], &addr)) {
    enrf_set_phy(BOOL_PARAM(1));
    VALIDATE_NRF(enrf_connect_to(&addr, discovery_handler, nus_c_response));
  } else {
    CMD_ERROR("Invalid mac address");
  }
}

//--------------------------------------------------------------------------

static void disconnect() {
  VALIDATE_NRF(enrf_disconnect());
}

//--------------------------------------------------------------------------

static void write(uint8_t op) {
  uint16_t len;
  uint16_t handle = strtoul(m_params[0], NULL, 16);
  bool ok = handle > 0;
  if (ok) {
    len = hex_to_bytes(m_params[1], m_data_buff, sizeof(m_data_buff));
    ok = len >= 1;
  }
  if (ok) {
    ret_code_t res;
    while ((res = enrf_write_char(op, handle, m_data_buff, len)) == NRF_ERROR_RESOURCES) {
      // Handle busy state
      sd_app_evt_wait();
    }
    VALIDATE_NRF(res);
  } else {
    CMD_ERROR("Syntax error");
  }
}

//--------------------------------------------------------------------------

const char *m_help =
    "\n=== ble_tool ===\n"
    "Syntax: command[;param]+\n"
    "Available commands:\n"
    "  vers                      Show version\n"
    "  tx_pow                    Set tx power\n"
    "    param pow_dbm\n"
    "  scan                      Start or stop scan\n"
    "    params: match_string;only_once;long_range;active;timeout\n"
    "    Empty params stops scan\n"
    "  advertise                 Start advertisement\n"
    "    params: name|manuf_data;connectable;long_range;timeout_s;interval_ms\n"
    "    Empty params stops advertisings\n"
    "  connect                   Connect to given address\n"
    "    params: mac_address;long_range\n"
    "  cancel_connect\n"
    "  disconnect\n"
    "  add_uid                   Add service or charact uuid\n"
    "    param: uuid_in_hex\n"
    "  notify                    Enable notifications\n"
    "    param: handle\n"
    "  write_cmd                 Write charact value\n"
    "    param: handle;value_in_hex\n"
    "  write                     Write charact value with response\n"
    "    param: handle;value_in_hex\n"
    "  read                      Read charact value\n"
    "    param: handle\n"
    "  nusc                      Write nus client string\n"
    "    param: string\n"
    "  restart                   Restart unit with possible dfu mode\n"
    "    param: 1|0\n"
    "  mac                       Show unit mac address\n"
    "";
#define CMD_EQ(str) ((strcasecmp(m_command, str) == 0))

static void handle_command() {
  m_param_cnt = 0;
  char *pos = m_command;
  // Split up the parameters
  memset(m_params, 0, sizeof(m_params));
  do {
    pos = strchr(pos, ';');
    if (pos)
      *(pos++) = 0;
    m_params[m_param_cnt] = pos;
  } while (pos && ++m_param_cnt < MAX_PARAMS);
  pos = m_command;
  while (*pos)
    *pos++ = toupper(*pos);
  if (CMD_EQ("vers")) {
    CMD_OK("%s %s", _build_version, _build_time);
  } else if (CMD_EQ("help")) {
    enrf_serial_write(m_help);
  } else if (CMD_EQ("tx_pow") && m_param_cnt) {
    enrf_set_tx_power(strtoul(m_params[0], NULL, 10));
    CMD_OK("");
  } else if (CMD_EQ("scan")) {
    scan();
  } else if (CMD_EQ("advertise")) {
    advertise();
  } else if (CMD_EQ("connect") && m_param_cnt) {
    connect();
  } else if (CMD_EQ("cancel_connect")) {
    VALIDATE_NRF(sd_ble_gap_connect_cancel());
  } else if (CMD_EQ("disconnect")) {
    disconnect();
  } else if (CMD_EQ("add_uuid") && m_param_cnt) {
    add_uuid();
  } else if (CMD_EQ("notify") && m_param_cnt) {
    VALIDATE_NRF(enrf_enable_char_notif(strtoul(m_params[0], NULL, 16), true));
  } else if (CMD_EQ("write_cmd") && m_param_cnt > 1) {
    write(BLE_GATT_OP_WRITE_CMD);
  } else if (CMD_EQ("write") && m_param_cnt > 1) {
    write(BLE_GATT_OP_WRITE_REQ);
  } else if (CMD_EQ("read") && m_param_cnt) {
    VALIDATE_NRF(enrf_read_char(strtoul(m_params[0], NULL, 16)));
  } else if (CMD_EQ("nusc")) {
    VALIDATE_NRF(enrf_nus_c_string_send(m_params[0]));
  } else if (CMD_EQ("restart")) {
    enrf_restart(BOOL_PARAM(0));
  } else if (CMD_EQ("mac")) {
    CMD_OK("%s", enrf_get_device_address());
  } else {
    RESP_ERROR("Invalid command: \"%s\" Type help for listing", m_command);
  }
}

//--------------------------------------------------------------------------

static void startup() {
  // Show the startup reason
  uint32_t reset_reason;
  sd_power_reset_reason_get(&reset_reason);
  sd_power_reset_reason_clr(0xFFFFFFFF);
  RESP_ASYNC("STARTUP:%lX", reset_reason);
}

//--------------------------------------------------------------------------

int main() {
  enrf_init("ble_tool", on_ble_evt);
  enrf_serial_enable(true);
  bsp_init(BSP_INIT_LEDS, NULL);
  startup();
  while (true) {
    enrf_wait_for_event();
    if (enrf_serial_read(m_command, sizeof(m_command))) {
      handle_command();
    }
  }
}