#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
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
  enum Request : uint8_t {
    REQ_ELECTRIC = 1,      // 87/69
    REQ_FLOW = 2,          // 93/290
    REQ_COUNTERS = 4,      // 93/1
    REQ_TEMPERATURE = 8,   // 93/300
    REQ_ENERGY = 16,       // 87/1
    REQ_OP_LOCAL = 32,     // 86/6 lokale Anforderung (App/Taste)
    REQ_OP_PRIO = 64,      // 86/7 priorisierte Anforderung -> Sensoren
    REQ_OP_REALIZED = 128  // 86/8 umgesetzte Anforderung
  };
  static constexpr uint8_t REQ_DIAG_MASK = REQ_ENERGY | REQ_OP_LOCAL | REQ_OP_PRIO | REQ_OP_REALIZED;
  static const char *req_name_(uint8_t req);

  void reset_state_();
  void write_cccd_();
  void send_frame_(const uint8_t *frame, size_t len);
  void send_next_request_();
  void handle_reply_(const geni::Reply &r);
  void publish_(sensor::Sensor *s, float value);
  void publish_or_nan_(sensor::Sensor *s, float value);  // NaN wird ebenfalls gesendet (Wert gilt gerade nicht)
  void publish_text_(text_sensor::TextSensor *s, const char *value);
  void publish_all_nan_();

  Stage stage_{Stage::IDLE};
  uint16_t data_handle_{0};
  uint16_t cccd_handle_{0};
  bool handles_ready_{false};
  bool enc_ok_{false};
  uint8_t cccd_tries_{0};
  uint8_t hello_sent_{0};
  uint32_t t_stage_{0};
  uint32_t t_last_tx_{0};
  bool inflight_{false};
  uint8_t last_req_{0};  // zuletzt gesendete Abfrage (es ist immer nur eine unterwegs)
  uint8_t wanted_mask_{0};
  uint8_t pending_mask_{0};
  geni::Reassembler rx_;
};

}  // namespace grundfos_alpha2go
}  // namespace esphome

#endif
