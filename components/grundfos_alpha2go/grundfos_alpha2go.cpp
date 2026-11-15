#include "grundfos_alpha2go.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include <cmath>
#include <cstdio>

#ifdef USE_ESP32

namespace esphome {
namespace grundfos_alpha2go {

static const char *const TAG = "grundfos_alpha2go";

static const uint32_t ENC_TIMEOUT_MS = 1500;    // so lange auf AUTH_CMPL warten, danach trotzdem weiter
static const uint32_t CCCD_TIMEOUT_MS = 2000;
static const uint32_t REPLY_TIMEOUT_MS = 1000;  // wie die GO-App (GENI_TIMEOUT)
static const uint32_t MIN_TX_GAP_MS = 30;       // Pumpe antwortet nach ca. 80 ms; nur eine Anfrage gleichzeitig

static const espbt::ESPBTUUID GENI_SERVICE = espbt::ESPBTUUID::from_uint16(0xFE5D);
static const espbt::ESPBTUUID GENI_CHAR = espbt::ESPBTUUID::from_raw("859cffd1-036e-432a-aa28-1a0085b87ba9");

void GrundfosAlpha2Go::dump_config() {
  ESP_LOGCONFIG(TAG, "Grundfos ALPHA2 GO");
  LOG_SENSOR("  ", "Flow", this->flow_sensor_);
  LOG_SENSOR("  ", "Head", this->head_sensor_);
  LOG_SENSOR("  ", "Power", this->power_sensor_);
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "DC voltage", this->dc_voltage_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Speed", this->speed_sensor_);
  LOG_SENSOR("  ", "Aux temperature", this->aux_temperature_sensor_);
  LOG_SENSOR("  ", "Media temperature", this->media_temperature_sensor_);
  LOG_SENSOR("  ", "PCB temperature", this->pcb_temperature_sensor_);
  LOG_SENSOR("  ", "Controlbox temperature", this->controlbox_temperature_sensor_);
  LOG_SENSOR("  ", "Starts", this->starts_sensor_);
  LOG_SENSOR("  ", "Operating hours", this->operating_hours_sensor_);
  LOG_SENSOR("  ", "Power-on hours", this->power_on_hours_sensor_);
  LOG_SENSOR("  ", "Energy", this->energy_sensor_);
  LOG_SENSOR("  ", "Operation mode", this->operation_mode_sensor_);
  LOG_SENSOR("  ", "Control mode", this->control_mode_sensor_);
  LOG_SENSOR("  ", "Setpoint (raw, SI)", this->setpoint_sensor_);
  LOG_SENSOR("  ", "Setpoint head", this->setpoint_head_sensor_);
  LOG_SENSOR("  ", "Setpoint flow", this->setpoint_flow_sensor_);
  LOG_SENSOR("  ", "Setpoint actual", this->setpoint_actual_sensor_);
  LOG_TEXT_SENSOR("  ", "Operation mode text", this->operation_mode_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Control mode text", this->control_mode_text_sensor_);
}

static constexpr float PA_PER_M = 9806.65f;  // 1 m Wassersaeule

// Abfragen in Sende-Reihenfolge
struct RequestDef {
  uint8_t bit;
  uint8_t obj;
  uint16_t sub;
  const char *name;
};
static const RequestDef REQUESTS[] = {
    {1, 87, 69, "87/69"},  {2, 93, 290, "93/290"}, {4, 93, 1, "93/1"},  {8, 93, 300, "93/300"},
    {16, 87, 1, "87/1"},   {32, 86, 6, "86/6"},    {64, 86, 7, "86/7"}, {128, 86, 8, "86/8"},
};

const char *GrundfosAlpha2Go::req_name_(uint8_t req) {
  for (const auto &r : REQUESTS)
    if (r.bit == req) return r.name;
  return "?";
}

// Betriebsart (Enum OperationMode im GENI-Profil): Name fuer das Log, Klartext fuer HA
struct OperationModeDef {
  const char *name;
  const char *text;
};
static const OperationModeDef OPERATION_MODES[] = {
    {"Auto", "Auto"},           {"Stop", "Stopp"},
    {"Min", "Minimum"},         {"Max", "Maximum"},
    {"UserDefined", "Benutzerdefiniert"},       {"ForcedStart", "Zwangsstart"},
    {"NoCmd", "Kein Befehl"},   {"Hand", "Hand"},
    {"Standby", "Standby"},     {"StopWithoutDelay", "Stopp sofort"},
    {"EmergencyRun", "Notbetrieb"},             {"UserDefined2", "Benutzerdefiniert 2"},
    {"SetPumpsToLocalMode", "Lokalbetrieb"},
};
static const OperationModeDef *operation_mode_def(uint8_t v) {
  return v < sizeof(OPERATION_MODES) / sizeof(OPERATION_MODES[0]) ? &OPERATION_MODES[v] : nullptr;
}
static const char *operation_mode_name(uint8_t v) {
  auto *m = operation_mode_def(v);
  return m != nullptr ? m->name : "?";
}

// Regelart (Enum ControlMode). kind: Einheit des set_point -> 0 unbekannt, 1 Druck (Pa), 2 Durchfluss (m3/s)
enum SetpointKind : uint8_t { SP_UNKNOWN = 0, SP_PRESSURE = 1, SP_FLOW = 2 };
struct ControlModeDef {
  uint8_t code;
  const char *name;
  const char *text;
  SetpointKind kind;
};
static const ControlModeDef CONTROL_MODES[] = {
    {0, "ConstantPressure", "Konstantdruck", SP_PRESSURE},
    {1, "ProportionalPressure", "Proportionaldruck", SP_PRESSURE},  // Pa an HK1 verifiziert
    {2, "ConstantSpeed", "Konstantdrehzahl", SP_UNKNOWN},
    {5, "AutoAdapt", "AutoAdapt", SP_PRESSURE},
    {6, "ConstantTemperature", "Konstanttemperatur", SP_UNKNOWN},
    {7, "ClosedLoopSensor", "Sensorregelung", SP_UNKNOWN},
    {8, "ConstantFlow", "Konstanter Durchfluss", SP_FLOW},  // m3/s an HK1 verifiziert
    {9, "ConstantLevel", "Konstantes Niveau", SP_UNKNOWN},
    {10, "FlowAdapt", "FlowAdapt", SP_FLOW},
    {11, "ConstantDifferentialPressure", "Konstanter Differenzdruck", SP_PRESSURE},
    {12, "ConstantDifferentialTemperature", "Konstante Differenztemperatur", SP_UNKNOWN},
    {13, "AutoAdaptRadiator", "AutoAdapt Heizkörper", SP_PRESSURE},
    {14, "AutoAdaptUnderfloor", "AutoAdapt Fußbodenheizung", SP_PRESSURE},
    {15, "AutoAdaptRadiatorAndUnderfloor", "AutoAdapt Heizkörper + Fußboden", SP_PRESSURE},
    {24, "UserDefined", "Benutzerdefiniert", SP_UNKNOWN},
    {26, "ProportionalDifferentialPressure", "Proportionaler Differenzdruck", SP_PRESSURE},
    {254, "None", "Keine", SP_UNKNOWN},
};
static const ControlModeDef *control_mode_def(uint8_t v) {
  for (const auto &m : CONTROL_MODES)
    if (m.code == v) return &m;
  return nullptr;
}
static const char *control_mode_name(uint8_t v) {
  auto *m = control_mode_def(v);
  return m != nullptr ? m->name : "?";
}
static SetpointKind setpoint_kind(uint8_t ctrl) {
  auto *m = control_mode_def(ctrl);
  return m != nullptr ? m->kind : SP_UNKNOWN;
}

void GrundfosAlpha2Go::setup() {
  if (this->flow_sensor_ != nullptr || this->head_sensor_ != nullptr) this->wanted_mask_ |= REQ_FLOW;
  if (this->power_sensor_ != nullptr || this->voltage_sensor_ != nullptr || this->dc_voltage_sensor_ != nullptr ||
      this->current_sensor_ != nullptr || this->speed_sensor_ != nullptr || this->aux_temperature_sensor_ != nullptr)
    this->wanted_mask_ |= REQ_ELECTRIC;
  if (this->starts_sensor_ != nullptr || this->operating_hours_sensor_ != nullptr ||
      this->power_on_hours_sensor_ != nullptr)
    this->wanted_mask_ |= REQ_COUNTERS;
  if (this->media_temperature_sensor_ != nullptr || this->pcb_temperature_sensor_ != nullptr ||
      this->controlbox_temperature_sensor_ != nullptr)
    this->wanted_mask_ |= REQ_TEMPERATURE;
  if (this->energy_sensor_ != nullptr) this->wanted_mask_ |= REQ_ENERGY;
  // 86/6 und 86/8 nur zum Vergleich im Log; die Sensoren kommen aus 86/7
  if (this->operation_mode_sensor_ != nullptr || this->control_mode_sensor_ != nullptr ||
      this->setpoint_sensor_ != nullptr || this->setpoint_head_sensor_ != nullptr ||
      this->setpoint_flow_sensor_ != nullptr || this->setpoint_actual_sensor_ != nullptr ||
      this->operation_mode_text_sensor_ != nullptr || this->control_mode_text_sensor_ != nullptr)
    this->wanted_mask_ |= REQ_OP_LOCAL | REQ_OP_PRIO | REQ_OP_REALIZED;
}

void GrundfosAlpha2Go::reset_state_() {
  this->stage_ = Stage::IDLE;
  this->handles_ready_ = false;
  this->enc_ok_ = false;
  this->cccd_tries_ = 0;
  this->hello_sent_ = 0;
  this->inflight_ = false;
  this->last_req_ = 0;
  this->pending_mask_ = 0;
  this->rx_.reset();
}

void GrundfosAlpha2Go::publish_(sensor::Sensor *s, float value) {
  if (s != nullptr && std::isfinite(value)) s->publish_state(value);
}

void GrundfosAlpha2Go::publish_or_nan_(sensor::Sensor *s, float value) {
  if (s != nullptr) s->publish_state(std::isfinite(value) ? value : NAN);
}

void GrundfosAlpha2Go::publish_text_(text_sensor::TextSensor *s, const char *value) {
  if (s != nullptr) s->publish_state(value);
}

void GrundfosAlpha2Go::publish_all_nan_() {
  sensor::Sensor *all[] = {this->flow_sensor_,
                           this->head_sensor_,
                           this->power_sensor_,
                           this->voltage_sensor_,
                           this->dc_voltage_sensor_,
                           this->current_sensor_,
                           this->speed_sensor_,
                           this->aux_temperature_sensor_,
                           this->media_temperature_sensor_,
                           this->pcb_temperature_sensor_,
                           this->controlbox_temperature_sensor_,
                           this->operation_mode_sensor_,
                           this->control_mode_sensor_,
                           this->setpoint_sensor_,
                           this->setpoint_head_sensor_,
                           this->setpoint_flow_sensor_,
                           this->setpoint_actual_sensor_};
  for (auto *s : all)
    if (s != nullptr) s->publish_state(NAN);
}

void GrundfosAlpha2Go::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (event != ESP_GAP_BLE_AUTH_CMPL_EVT) return;
  if (memcmp(param->ble_security.auth_cmpl.bd_addr, this->parent_->get_remote_bda(), 6) != 0) return;
  if (param->ble_security.auth_cmpl.success) {
    ESP_LOGD(TAG, "[%s] Link encrypted", this->parent_->address_str());
    this->enc_ok_ = true;
  } else {
    ESP_LOGW(TAG, "[%s] Authentication failed (reason=%d). Reason 82 (0x52): pump is not in pairing mode - press the "
                  "Bluetooth button on the pump while the ESP connects.",
             this->parent_->address_str(), param->ble_security.auth_cmpl.fail_reason);
  }
}

void GrundfosAlpha2Go::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                           esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_CONNECT_EVT: {
      if (memcmp(param->connect.remote_bda, this->parent_->get_remote_bda(), 6) != 0) return;
      this->reset_state_();
      // Die Pumpe trennt unverschluesselte Clients nach ca. 2 s -> sofort Verschluesselung (gespeicherter Bond).
      esp_err_t r = esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT);
      if (r != ESP_OK) ESP_LOGW(TAG, "esp_ble_set_encryption failed (%d)", (int) r);
      this->stage_ = Stage::WAIT_ENC;
      this->t_stage_ = millis();
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *chr = this->parent_->get_characteristic(GENI_SERVICE, GENI_CHAR);
      if (chr == nullptr) {
        ESP_LOGE(TAG, "[%s] GENI characteristic not found - not a Grundfos pump with Bluetooth GENI?",
                 this->parent_->address_str());
        break;
      }
      auto *cccd = chr->get_descriptor(0x2902);
      if (cccd == nullptr) {
        ESP_LOGE(TAG, "[%s] CCCD of GENI characteristic not found", this->parent_->address_str());
        break;
      }
      this->data_handle_ = chr->handle;
      this->cccd_handle_ = cccd->handle;
      this->handles_ready_ = true;
      ESP_LOGD(TAG, "[%s] GENI handles: data=0x%04x cccd=0x%04x", this->parent_->address_str(), this->data_handle_,
               this->cccd_handle_);
      auto status = this->parent_->register_for_notify(this->data_handle_);
      if (status != ESP_OK) ESP_LOGW(TAG, "register_for_notify failed (%d)", (int) status);
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      // Handles sind gemerkt -> GATT-Cache darf freigegeben werden.
      this->node_state = espbt::ClientState::ESTABLISHED;
      break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT: {
      if (param->write.handle != this->cccd_handle_) break;
      if (param->write.status == ESP_GATT_OK) {
        ESP_LOGD(TAG, "[%s] Notifications enabled", this->parent_->address_str());
        this->stage_ = Stage::HELLO;
        this->t_stage_ = millis();
      } else {
        ESP_LOGW(TAG, "[%s] CCCD write failed (status %d) - will retry", this->parent_->address_str(),
                 (int) param->write.status);
      }
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->data_handle_) break;
      ESP_LOGVV(TAG, "RX %d bytes", (int) param->notify.value_len);
      if (this->inflight_ && (this->last_req_ & REQ_DIAG_MASK))  // auch Antworten, die der Parser verwirft
        ESP_LOGV(TAG, "%s raw RX: %s", req_name_(this->last_req_),
                 format_hex_pretty(param->notify.value, param->notify.value_len).c_str());
      int crc_errors = this->rx_.feed(param->notify.value, param->notify.value_len,
                                      [this](const geni::Reply &r) { this->handle_reply_(r); });
      if (crc_errors > 0) ESP_LOGW(TAG, "%d GENI frame(s) dropped: CRC error", crc_errors);
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      this->node_state = espbt::ClientState::IDLE;
      this->reset_state_();
      this->publish_all_nan_();
      break;
    }
    default:
      break;
  }
}

void GrundfosAlpha2Go::write_cccd_() {
  uint8_t on[] = {0x01, 0x00};
  auto rc = esp_ble_gattc_write_char_descr(this->parent_->get_gattc_if(), this->parent_->get_conn_id(),
                                           this->cccd_handle_, sizeof(on), on, ESP_GATT_WRITE_TYPE_RSP,
                                           ESP_GATT_AUTH_REQ_NONE);
  if (rc != ESP_OK) ESP_LOGW(TAG, "CCCD write request failed (%d)", (int) rc);
  this->cccd_tries_++;
  this->t_stage_ = millis();
}

void GrundfosAlpha2Go::send_frame_(const uint8_t *frame, size_t len) {
  uint8_t tmp[16];
  if (len > sizeof(tmp)) return;
  memcpy(tmp, frame, len);
  auto st = esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->data_handle_,
                                     len, tmp, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (st != ESP_OK) ESP_LOGW(TAG, "write failed (%d)", (int) st);
  this->t_last_tx_ = millis();
}

void GrundfosAlpha2Go::send_next_request_() {
  for (const auto &r : REQUESTS) {
    if (!(this->pending_mask_ & r.bit)) continue;
    this->pending_mask_ &= ~r.bit;
    uint8_t f[16];
    size_t n = geni::build_object_get(f, r.obj, r.sub);
    ESP_LOGVV(TAG, "TX %s: %s", r.name, format_hex_pretty(f, n).c_str());
    this->last_req_ = r.bit;
    this->inflight_ = true;
    this->send_frame_(f, n);
    return;
  }
}

void GrundfosAlpha2Go::loop() {
  uint32_t now = millis();
  switch (this->stage_) {
    case Stage::WAIT_ENC:
      if (this->handles_ready_ && (this->enc_ok_ || now - this->t_stage_ > ENC_TIMEOUT_MS)) {
        if (!this->enc_ok_) ESP_LOGW(TAG, "No AUTH_CMPL seen, continuing anyway");
        this->stage_ = Stage::WAIT_CCCD;
        this->write_cccd_();
      }
      break;
    case Stage::WAIT_CCCD:
      if (now - this->t_stage_ > CCCD_TIMEOUT_MS && this->cccd_tries_ < 3) this->write_cccd_();
      break;
    case Stage::HELLO: {
      // Erstabfrage der GO-App (zweimal): Class 2, IDs 148/149/150 = unit_family/type/version
      if (this->hello_sent_ < 2 && now - this->t_last_tx_ >= MIN_TX_GAP_MS) {
        static const uint8_t ids[] = {148, 149, 150};
        uint8_t f[16];
        size_t n = geni::build_class2_get(f, ids, sizeof(ids));
        this->send_frame_(f, n);
        this->hello_sent_++;
      } else if (this->hello_sent_ >= 2 && now - this->t_last_tx_ >= MIN_TX_GAP_MS) {
        this->stage_ = Stage::POLLING;
        ESP_LOGI(TAG, "[%s] Connected, polling", this->parent_->address_str());
        this->pending_mask_ |= this->wanted_mask_;
      }
      break;
    }
    case Stage::POLLING:
      if (this->inflight_ && now - this->t_last_tx_ > REPLY_TIMEOUT_MS) {
        ESP_LOGD(TAG, "Reply timeout (%s)", req_name_(this->last_req_));
        this->inflight_ = false;
      }
      if (!this->inflight_ && this->pending_mask_ != 0 && now - this->t_last_tx_ >= MIN_TX_GAP_MS)
        this->send_next_request_();
      break;
    default:
      break;
  }
}

void GrundfosAlpha2Go::update() {
  if (this->stage_ == Stage::POLLING) this->pending_mask_ |= this->wanted_mask_;
}

void GrundfosAlpha2Go::handle_reply_(const geni::Reply &r) {
  const uint8_t req = this->inflight_ ? this->last_req_ : 0;  // zu welcher Abfrage die Antwort gehoert
  this->inflight_ = false;
  const uint8_t *d = r.data;
  ESP_LOGV(TAG, "Reply type=%u version=%u len=%u", r.type, r.version, r.len);
  switch (r.type) {
    case geni::TYPE_PROCESS_VALUES_EXT:  // 93/290, v2, 36 Byte
      if (r.len >= 32) {
        // Pumpe liefert Flow/Head fertig umgerechnet (auf ALPHA2 GO HK1 verifiziert)
        this->publish_(this->flow_sensor_, geni::be_float(d + 24));  // m3/h
        this->publish_(this->head_sensor_, geni::be_float(d + 28));  // m
      } else if (r.len >= 8) {
        // Rueckfall fuer kuerzere Antworten: @0 in m3/s, @4 in Pa
        this->publish_(this->flow_sensor_, geni::be_float(d + 0) * 3600.0f);   // m3/s -> m3/h
        this->publish_(this->head_sensor_, geni::be_float(d + 4) / 9806.65f);  // Pa -> m Wassersaeule
      }
      if (r.len >= 36) {
        ESP_LOGV(TAG, "565: f0=%g f4=%g f8=%g f12=%g f16=%g f20=%g", geni::be_float(d + 0), geni::be_float(d + 4),
                 geni::be_float(d + 8), geni::be_float(d + 12), geni::be_float(d + 16), geni::be_float(d + 20));
        ESP_LOGV(TAG, "565: flow_m3h=%.3f head_m=%.3f kv=%.3f (Felder @24/28/32)", geni::be_float(d + 24),
                 geni::be_float(d + 28), geni::be_float(d + 32));
      }
      break;
    case geni::TYPE_MOTOR_DETAILS:  // 87/69, v1 (37 B) / v2/v3 (41 B)
      if (r.len >= 24) {
        this->publish_(this->voltage_sensor_, geni::be_float(d + 0));
        this->publish_(this->dc_voltage_sensor_, geni::be_float(d + 4));
        this->publish_(this->current_sensor_, geni::be_float(d + 8));
        this->publish_(this->power_sensor_, geni::be_float(d + 12));
        this->publish_(this->speed_sensor_, geni::be_float(d + 20));
      }
      if (r.len >= 36) this->publish_(this->aux_temperature_sensor_, geni::be_float(d + 32));
      if (r.len >= 36) {
        ESP_LOGV(TAG, "256: ac=%g dc=%g i=%g p=%g pdc=%g n=%g t24=%g t28=%g t32=%g", geni::be_float(d + 0),
                 geni::be_float(d + 4), geni::be_float(d + 8), geni::be_float(d + 12), geni::be_float(d + 16),
                 geni::be_float(d + 20), geni::be_float(d + 24), geni::be_float(d + 28), geni::be_float(d + 32));
      }
      break;
    case geni::TYPE_OPERATION_HISTORY:  // 93/1, v2, 28 Byte
      ESP_LOGV(TAG, "248: %s", format_hex_pretty(d, r.len).c_str());
      if (r.len >= 12) {
        if (this->starts_sensor_ != nullptr) this->starts_sensor_->publish_state((float) geni::be_u32(d + 0));
        if (this->operating_hours_sensor_ != nullptr)
          this->operating_hours_sensor_->publish_state((float) geni::be_u32(d + 8) / 3600.0f);
      }
      if (r.len >= 28 && this->power_on_hours_sensor_ != nullptr)
        this->power_on_hours_sensor_->publish_state((float) geni::be_u32(d + 24) / 3600.0f);
      break;
    case geni::TYPE_MEDIA_TEMPERATURE:  // 93/300
      ESP_LOGV(TAG, "534: %s", format_hex_pretty(d, r.len).c_str());
      if (r.len >= 12) {
        ESP_LOGV(TAG, "534: f0=%g f4=%g f8=%g", geni::be_float(d + 0), geni::be_float(d + 4), geni::be_float(d + 8));
      }
      if (r.len >= 12) {
        this->publish_(this->media_temperature_sensor_, geni::be_float(d + 0));
        this->publish_(this->pcb_temperature_sensor_, geni::be_float(d + 4));
        this->publish_(this->controlbox_temperature_sensor_, geni::be_float(d + 8));
      }
      break;
    case geni::TYPE_ENERGY: {  // 87/1, v1: double@0 J, float@8 W, u16@12 n, n x {float W, double J}
      if (r.len < 12) break;
      const double joule = geni::be_double(d + 0);
      ESP_LOGV(TAG, "232: system_energy=%.0f J (%.4f kWh) system_power=%.2f W", joule, joule / 3.6e6,
               geni::be_float(d + 8));
      if (r.len >= 14) {
        const uint16_t pumps = geni::be_u16(d + 12);
        for (uint16_t i = 0; i < pumps && 14u + 12u * (i + 1u) <= r.len; i++)
          ESP_LOGV(TAG, "232: pump %u power=%.2f W energy=%.0f J", i, geni::be_float(d + 14 + 12 * i),
                   geni::be_double(d + 18 + 12 * i));
      }
      if (std::isfinite(joule) && joule >= 0) this->publish_(this->energy_sensor_, (float) (joule / 3.6e6));
      break;
    }
    case geni::TYPE_OPERATION_STATUS: {  // 86/6, 86/7, 86/8 (gleicher Typ -> Zuordnung ueber die Abfrage)
      if (r.len < 7) break;
      const uint8_t src = d[0], mode = d[1], ctrl = d[2];
      const float sp = geni::be_float(d + 3);
      ESP_LOGD(TAG, "%s: source=%u mode=%u (%s) control=%u (%s) setpoint=%g", req_name_(req), src, mode,
               operation_mode_name(mode), ctrl, control_mode_name(ctrl), sp);
      const SetpointKind kind = setpoint_kind(ctrl);
      if (req == REQ_OP_PRIO) {
        this->publish_(this->operation_mode_sensor_, mode);
        this->publish_(this->control_mode_sensor_, ctrl);
        this->publish_(this->setpoint_sensor_, sp);
        // Sensoren mit fester Einheit; gilt die Einheit fuer die aktuelle Regelart nicht, wird NaN gemeldet
        this->publish_or_nan_(this->setpoint_head_sensor_, kind == SP_PRESSURE ? sp / PA_PER_M : NAN);
        this->publish_or_nan_(this->setpoint_flow_sensor_, kind == SP_FLOW ? sp * 3600.0f : NAN);
        auto *om = operation_mode_def(mode);
        auto *cm = control_mode_def(ctrl);
        char buf[24];
        if (om == nullptr) snprintf(buf, sizeof(buf), "Unbekannt (%u)", mode);
        this->publish_text_(this->operation_mode_text_sensor_, om != nullptr ? om->text : buf);
        if (cm == nullptr) snprintf(buf, sizeof(buf), "Unbekannt (%u)", ctrl);
        this->publish_text_(this->control_mode_text_sensor_, cm != nullptr ? cm->text : buf);
      } else if (req == REQ_OP_REALIZED) {
        this->publish_or_nan_(this->setpoint_actual_sensor_, kind == SP_PRESSURE ? sp / PA_PER_M : NAN);
      }
      break;
    }
    default:
      ESP_LOGD(TAG, "Unknown reply type %u (version %u, %u bytes): %s", r.type, r.version, r.len,
               format_hex_pretty(d, r.len).c_str());
      break;
  }
}

}  // namespace grundfos_alpha2go
}  // namespace esphome

#endif
