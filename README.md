# ESPHome component: Grundfos ALPHA2 GO (BLE)

**English** · [Deutsch](README.de.md)

External ESPHome component `grundfos_alpha2go` that reads and controls a Grundfos **ALPHA2 GO** circulator pump over Bluetooth LE, no Grundfos app needed.
ESPHome's built-in `alpha3` component does not work with the ALPHA2 GO: it polls an object for flow and head that only the original ALPHA3 has.

**Status:** Tested reading and writing with an ALPHA2 GO 25-60 180. Three pumps (2× ALPHA2 GO 25-60 180, 1× ALPHA2 GO 25-40 180) run at the same time on one ESP32. Builds with ESPHome 2026.10.2 (ESP-IDF and Arduino).
The protocol was derived from the Grundfos GO app and the capture in `Lasse-source/esphome-grundfos-alpha2-go`.

## Quick start

```yaml
external_components:
  - source: github://CDank/esphome-grundfos-alpha2go-component@main
    components: [grundfos_alpha2go]

esp32_ble_tracker:

ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF"   # pump MAC (shows up as "ALPHA2 GO" in a scan)
    id: pump_ble

sensor:
  - platform: grundfos_alpha2go
    id: pump
    ble_client_id: pump_ble
    language: en                       # texts in English (default: de)
    flow: {name: "Pump flow"}
    head: {name: "Pump head"}
    power: {name: "Pump power"}
    media_temperature: {name: "Pump media temperature"}
    connected: {name: "Pump connected"}
```

A full example with every entity is in [`example/grundfos-alpha2go.yaml`](example/grundfos-alpha2go.yaml).

1. Flash the ESP. **Press the Bluetooth button on the pump while the ESP connects for the first time**, otherwise the pump rejects it (`reason=82`). The bond is stored in the ESP's flash and survives reboots and OTA updates.
2. Close the Grundfos GO app on your phone. The pump accepts only one client at a time.

Several pumps: add one `ble_client` and one `grundfos_alpha2go` block per pump and pair each pump with its Bluetooth button. Three pumps on one ESP32 are tested; ESPHome allows 3 BLE connections by default, more need `esp32_ble: max_connections:` (ESP-IDF, up to 9).

> **Note:** Binary sensors and text sensors of this component (`connected`, `night_mode`, `venting_status` …) go into the same `sensor:` block as the numeric sensors, not under `binary_sensor:` or `text_sensor:`.

## Sensors

Only configured values are polled, every 10 s by default (`update_interval`). Settings such as night setback and air detection are read every 6th poll.

| Option | Unit | Description |
|---|---|---|
| `flow` | m³/h | Flow |
| `head` | m | Head |
| `power` | W | Power consumption |
| `speed` | rpm | Motor speed |
| `voltage` / `dc_voltage` | V | Mains voltage / DC link voltage |
| `current` | A | Current |
| `media_temperature` | °C | Water temperature |
| `aux_temperature` / `pcb_temperature` / `controlbox_temperature` | °C | Electronics temperatures |
| `energy` | kWh | The pump's own energy counter (same as in the app) |
| `starts` | – | Number of starts |
| `operating_hours` / `power_on_hours` | h | Running hours / powered hours |
| `operation_mode` / `operation_mode_text` | code / text | Auto, Stop, Min, Max … |
| `control_mode` / `control_mode_text` | code / text | Proportional pressure, constant pressure, … |
| `setpoint_head` | m | Setpoint in pressure modes (incl. AutoAdapt) |
| `setpoint_flow` | m³/h | Setpoint in constant flow mode |
| `setpoint_speed` | rpm | Setpoint in constant speed mode |
| `setpoint_actual` | m | Current target the pump is regulating to (point on the curve) |
| `setpoint` | SI | Raw setpoint (Pa, m³/s or rpm) |
| `night_setback_enabled` | binary | Night setback is enabled |
| `night_mode` | binary | Pump is in night mode right now |
| `air_detection_enabled` | binary | Continuous air detection is enabled |
| `air_detections` | count | Air detections |
| `venting_active` / `venting_status` | binary / text | Venting running: pump only or system and pump |
| `connected` | binary | Connection is encrypted and the pump is being polled |

Only the `setpoint_*` sensor that matches the active control mode has a value, the others are "unknown".
In AutoAdapt modes the pump picks the setpoint itself, and `setpoint_actual` shows the same value as the setpoint.

`operation_mode` codes: 0 Auto, 1 Stop, 2 Min, 3 Max, 4 User defined, 7 Hand, 8 Standby.
`control_mode` codes: 0 constant pressure, 1 proportional pressure, 2 constant speed, 8 constant flow, 13 proportional pressure (AutoAdapt), 14 constant pressure (AutoAdapt).
**Language:** `language: en` in the sensor block switches the `_text` sensors, `venting_status` and the control mode select to English (default `de`). Home Assistant's own language can't be used: the ESPHome API doesn't tell the device which language HA runs in, and HA doesn't translate ESPHome text states.

## Controls

```yaml
switch:
  - platform: grundfos_alpha2go
    grundfos_alpha2go_id: pump   # id of the sensor block; can be left out with a single pump
    pump: {name: "Pump"}
    night_setback: {name: "Pump night setback"}
    air_detection: {name: "Pump air detection"}

select:
  - platform: grundfos_alpha2go
    control_mode: {name: "Pump control mode"}

number:
  - platform: grundfos_alpha2go
    setpoint_head: {name: "Pump setpoint head", mode: box}   # proportional / constant pressure
    setpoint_flow: {name: "Pump setpoint flow"}              # constant flow
    setpoint_speed: {name: "Pump setpoint speed"}            # constant speed

button:
  - platform: grundfos_alpha2go
    venting_start: {name: "Pump system venting start"}
    venting_stop: {name: "Pump system venting stop"}
```

| Entity | Effect |
|---|---|
| `pump` | On = operation mode Auto, off = Stop |
| `night_setback` | Night setback (AutoNight) on/off |
| `air_detection` | Continuous air detection on/off |
| `control_mode` | Proportional pressure, constant pressure, both with AutoAdapt, constant flow, constant speed |
| `setpoint_head` / `setpoint_flow` / `setpoint_speed` | Setpoint in m / m³/h / rpm; only the one matching the active control mode can be changed |
| `venting_start` / `venting_stop` | Start or stop the system venting manually |

- The displayed state always comes from the pump: after each write the component reads it back. Changes made in the app or on the pump show up with the next poll. The ESP writes nothing at boot.
- Default limits are 0.5–6 m, 0.1–3.5 m³/h and 1000–5500 rpm. Change them with `min_value`, `max_value` and `step`, e.g. `max_value: 4` for a 25-40.
- When the control mode changes unit, the last known setpoint for that unit is sent along, or 2 m, 1 m³/h or 3000 rpm if there is none yet.
- In AutoAdapt modes the setpoint is shown but can't be changed.
- If the pump rejects a command, the log shows `rejected by pump` and the entity falls back to the real state.
- The writes change the pump's local request, just like the GO app, so the app can change everything back afterwards.

## Connection loss

After a disconnect, all sensors, the select and the numbers turn "unknown" in Home Assistant. Switches keep their last state, because Home Assistant's ESPHome integration ignores the "no state" flag for switches. Use `connected` to tell whether they are current.

Sometimes the ESP's BLE stack loses the bond from RAM after a disconnect while it is still in flash, and every reconnect then fails with `reason=82`. The component detects this and restarts the BLE stack (`Restarting BLE stack to reload bonds from NVS`, at most every 30 s). The pump then reconnects without pressing its button. The restart affects all BLE functions of this ESP: connections to other pumps drop for about 8 s and come back on their own.

## Troubleshooting

```yaml
logger:
  level: VERBOSE
  logs:
    grundfos_alpha2go: VERBOSE
    esp32_ble: INFO
    esp32_ble_client: INFO
    ble_client: INFO
```

On `DEBUG` the component logs decoded status and settings, timeouts with the object number, and each disconnect with its reason (0x08 radio timeout, 0x13 closed by the pump, 0x16 closed by the ESP). `VERBOSE` adds every raw field of every reply.

For your own experiments, `id(pump).send_raw_class10({0x03, 0x5A, 0x00, 0x08});` sends any GENI class 10 APDU, for example from a template button. The reply is checked and logged.

Protocol details (object layout, write format) are in [`docs/protocol.md`](docs/protocol.md).

## Tests

```bash
g++ -std=c++17 -I components/grundfos_alpha2go tests/test_geni.cpp -o t && ./t
```

## Open points

- "Vent pump only" can't be started yet. It only shows up as control source 111 in object 86/8.
- Not tested: other firmware versions and ALPHA2 GO variants other than 25-60 180 and 25-40 180.

License: MIT. Independent project, not affiliated with Grundfos.
