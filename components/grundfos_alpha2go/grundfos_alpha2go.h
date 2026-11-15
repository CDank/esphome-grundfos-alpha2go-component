#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "geni.h"

#ifdef USE_ESP32

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>

namespace esphome {
namespace grundfos_alpha2go {

namespace espbt = esphome::esp32_ble_tracker;

class GrundfosAlpha2Go : public ble_client::BLEClientNode, public PollingComponent {
  SUB_SENSOR(flow)
  SUB_SENSOR(head)
  SUB_SENSOR(power)
  SUB_SENSOR(voltage)
  SUB_SENSOR(dc_voltage)
  SUB_SENSOR(current)
  SUB_SENSOR(speed)
  SUB_SENSOR(aux_temperature)
  SUB_SENSOR(media_temperature)
  SUB_SENSOR(pcb_temperature)
  SUB_SENSOR(controlbox_temperature)
  SUB_SENSOR(starts)
  SUB_SENSOR(operating_hours)
  SUB_SENSOR(power_on_hours)
  SUB_SENSOR(energy)          // 87/1 system_energy (J) -> kWh
  SUB_SENSOR(operation_mode)  // 86/7 operation_mode (0=Auto, 1=Stop, 2=Min, 3=Max, ...)
  SUB_SENSOR(control_mode)    // 86/7 control_mode (1=PP, 2=CS, 5=AutoAdapt, 13..15=AutoAdapt-Varianten, ...)
  SUB_SENSOR(setpoint)        // 86/7 set_point, Rohwert in SI (Pa bzw. m3/s)
  SUB_SENSOR(setpoint_head)   // 86/7 set_point in m, nur bei Druck-Regelarten
  SUB_SENSOR(setpoint_flow)   // 86/7 set_point in m3/h, nur bei Durchfluss-Regelart
  SUB_SENSOR(setpoint_speed)  // 86/7 set_point in 1/min, nur bei Konstantdrehzahl
  SUB_SENSOR(setpoint_actual) // 86/8 momentaner Zielwert in m, nur bei Druck-Regelarten
  SUB_TEXT_SENSOR(operation_mode)  // Klartext zu operation_mode
  SUB_TEXT_SENSOR(control_mode)    // Klartext zu control_mode
  SUB_SENSOR(air_detections)       // 88/604 Lufterkennungen nach 168 Betriebsstunden
  SUB_TEXT_SENSOR(venting_status)  // 88/612 Entlueftung: Inaktiv / Manuell / Automatisch
  SUB_BINARY_SENSOR(night_setback_enabled)  // 90/8 Nachtabsenkung aktiviert
  SUB_BINARY_SENSOR(night_mode)             // 90/7 gerade Nachtbetrieb
  SUB_BINARY_SENSOR(air_detection_enabled)  // 88/600 kontinuierliche Lufterkennung aktiviert
  SUB_BINARY_SENSOR(venting_active)         // 88/612 Entlueftung laeuft

 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;

 protected:
  enum class Stage : uint8_t {
    IDLE,        // nicht verbunden
    WAIT_ENC,    // Verschluesselung angefordert, warte auf AUTH_CMPL und Service-Suche
    WAIT_CCCD,   // CCCD geschrieben, warte auf Bestaetigung
    HELLO,       // Erstabfrage der GO-App senden
    POLLING,     // normaler Betrieb
  };
  enum Request : uint16_t {
    REQ_ELECTRIC = 1 << 0,       // 87/69
    REQ_FLOW = 1 << 1,           // 93/290
    REQ_COUNTERS = 1 << 2,       // 93/1
    REQ_TEMPERATURE = 1 << 3,    // 93/300
    REQ_ENERGY = 1 << 4,         // 87/1
    REQ_OP_LOCAL = 1 << 5,       // 86/6 lokale Anforderung (App/Taste)
    REQ_OP_PRIO = 1 << 6,        // 86/7 priorisierte Anforderung -> Sensoren
    REQ_OP_REALIZED = 1 << 7,    // 86/8 umgesetzte Anforderung
    REQ_NIGHT_ENABLE = 1 << 8,   // 90/8 Nachtabsenkung aktiviert
    REQ_NIGHT_STATUS = 1 << 9,   // 90/7 gerade Nachtbetrieb
    REQ_AIR_CONFIG = 1 << 10,    // 88/600 Lufterkennung konfiguriert
    REQ_AIR_COUNTER = 1 << 11,   // 88/604 Zaehler Lufterkennungen
    REQ_VENTING = 1 << 12,       // 88/612 Entlueftung Status
  };
  // Neue bzw. noch nicht an der Pumpe gepruefte Objekte: rohe Notifications auf VERBOSE loggen
  static constexpr uint16_t REQ_DIAG_MASK = REQ_ENERGY | REQ_OP_LOCAL | REQ_OP_PRIO | REQ_OP_REALIZED |
                                            REQ_NIGHT_ENABLE | REQ_NIGHT_STATUS | REQ_AIR_CONFIG |
                                            REQ_AIR_COUNTER | REQ_VENTING;
  // Einstellungen, die sich selten aendern: nur bei jedem SLOW_EVERY-ten update() (und nach dem Verbinden)
  static constexpr uint16_t REQ_SLOW_MASK = REQ_NIGHT_ENABLE | REQ_AIR_CONFIG | REQ_AIR_COUNTER;
  static constexpr uint8_t SLOW_EVERY = 6;
  static const char *req_name_(uint16_t req);

  void reset_state_();
  void write_cccd_();
  void send_frame_(const uint8_t *frame, size_t len);
  void send_next_request_();
  void handle_reply_(const geni::Reply &r);
  void publish_(sensor::Sensor *s, float value);
  void publish_or_nan_(sensor::Sensor *s, float value);  // NaN wird ebenfalls gesendet (Wert gilt gerade nicht)
  void publish_text_(text_sensor::TextSensor *s, const char *value);
  void publish_bin_(binary_sensor::BinarySensor *s, bool value);
  void publish_venting_();
  void start_reconnect_backoff_();
  void publish_all_nan_();

  Stage stage_{Stage::IDLE};
  uint16_t data_handle_{0};
  uint16_t cccd_handle_{0};
  bool handles_ready_{false};
  bool enc_ok_{false};
  bool auth_failed_{false};      // Verschluesselung in dieser Verbindung abgelehnt
  uint8_t auth_fail_count_{0};   // aufeinanderfolgende Ablehnungen ueber Verbindungen hinweg  uint8_t cccd_tries_{0};
  uint8_t hello_sent_{0};
  uint32_t t_stage_{0};
  uint32_t t_last_tx_{0};
  bool inflight_{false};
  uint16_t last_req_{0};  // zuletzt gesendete Abfrage (es ist immer nur eine unterwegs)
  uint16_t wanted_mask_{0};
  uint16_t pending_mask_{0};
  uint8_t update_count_{0};
  uint8_t system_venting_state_{0};  // 88/612: 0 inaktiv, 1 manuell, 2 automatisch
  bool pump_venting_{false};         // 86/8 Steuerquelle AirVenting
  geni::Reassembler rx_;
};

}  // namespace grundfos_alpha2go
}  // namespace esphome

#endif
