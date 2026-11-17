# ESPHome-Komponente: Grundfos ALPHA2 GO (BLE)

[English](README.md) · **Deutsch**

Externe ESPHome-Komponente `grundfos_alpha2go`, die eine Grundfos-Umwälzpumpe **ALPHA2 GO** über Bluetooth LE ausliest und steuert, ohne die Grundfos-App.
Die in ESPHome enthaltene `alpha3`-Komponente funktioniert mit der ALPHA2 GO nicht: Sie fragt Durchfluss und Förderhöhe über ein Objekt ab, das nur die ursprüngliche ALPHA3 hat.

**Status:** Getestet mit einer ALPHA2 GO 25-60 180, Lesen und Schreiben. Baut mit ESPHome 2026.10.2 (ESP-IDF und Arduino).
Das Protokoll stammt aus der Grundfos-GO-App und dem Mitschnitt von `Lasse-source/esphome-grundfos-alpha2-go`.

## Schnellstart

```yaml
external_components:
  - source: github://CDank/esphome-grundfos-alpha2go-component@main
    components: [grundfos_alpha2go]

esp32_ble_tracker:

ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF"   # MAC der Pumpe (im Scan als "ALPHA2 GO")
    id: pumpe_ble

sensor:
  - platform: grundfos_alpha2go
    id: pumpe
    ble_client_id: pumpe_ble
    flow: {name: "Pumpe Durchfluss"}
    head: {name: "Pumpe Förderhöhe"}
    power: {name: "Pumpe Leistung"}
    media_temperature: {name: "Pumpe Medientemperatur"}
    connected: {name: "Pumpe verbunden"}
```

Ein vollständiges Beispiel mit allen Entitäten steht in [`example/grundfos-alpha2go.yaml`](example/grundfos-alpha2go.yaml).

1. ESP flashen. **Beim ersten Verbinden die Bluetooth-Taste an der Pumpe drücken**, sonst lehnt die Pumpe ab (`reason=82`). Der Bond liegt danach im Flash des ESP und übersteht Neustarts und OTA-Updates.
2. Die GO-App auf dem Handy schließen. Die Pumpe akzeptiert nur einen Client gleichzeitig.

Mehrere Pumpen: je Pumpe ein eigener `ble_client` und ein eigener `grundfos_alpha2go`-Block; jede Pumpe einzeln per Bluetooth-Taste koppeln.

> **Hinweis:** Binär- und Textsensoren der Komponente (`connected`, `night_mode`, `venting_status` …) gehören in denselben `sensor:`-Block wie die Messwerte, nicht unter `binary_sensor:` oder `text_sensor:`.

## Sensoren

Abgefragt wird nur, was konfiguriert ist, standardmäßig alle 10 s (`update_interval`). Einstellungen wie Nachtabsenkung und Lufterkennung werden bei jeder 6. Abfrage gelesen.

| Option | Einheit | Beschreibung |
|---|---|---|
| `flow` | m³/h | Durchfluss |
| `head` | m | Förderhöhe |
| `power` | W | Leistungsaufnahme |
| `speed` | 1/min | Drehzahl |
| `voltage` / `dc_voltage` | V | Netzspannung / Zwischenkreisspannung |
| `current` | A | Strom |
| `media_temperature` | °C | Medientemperatur |
| `aux_temperature` / `pcb_temperature` / `controlbox_temperature` | °C | Elektroniktemperaturen |
| `energy` | kWh | Energiezähler der Pumpe (wie in der App) |
| `starts` | – | Anzahl Starts |
| `operating_hours` / `power_on_hours` | h | Betriebsstunden / Einschaltstunden |
| `operation_mode` / `operation_mode_text` | Code / Text | Auto, Stopp, Min, Max … |
| `control_mode` / `control_mode_text` | Code / Text | Proportionaldruck, Konstantdruck, … |
| `setpoint_head` | m | Sollwert bei Druck-Regelarten (auch AutoAdapt) |
| `setpoint_flow` | m³/h | Sollwert bei Konstantem Förderstrom |
| `setpoint_speed` | 1/min | Sollwert bei Konstantdrehzahl |
| `setpoint_actual` | m | Momentaner Zielwert der Pumpe (Punkt auf der Kurve) |
| `setpoint` | SI | Sollwert roh (Pa, m³/s oder 1/min) |
| `night_setback_enabled` | binär | Nachtabsenkung aktiviert |
| `night_mode` | binär | Pumpe ist gerade im Nachtbetrieb |
| `air_detection_enabled` | binär | Kontinuierliche Lufterkennung aktiviert |
| `air_detections` | Anzahl | Lufterkennungen |
| `venting_active` / `venting_status` | binär / Text | Entlüftung läuft: nur Pumpe oder Anlage und Pumpe |
| `connected` | binär | Verbindung verschlüsselt, Pumpe wird abgefragt |

Nur der `setpoint_*`-Sensor, der zur aktiven Regelart passt, hat einen Wert; die anderen stehen auf „unbekannt“.
Bei AutoAdapt bestimmt die Pumpe den Sollwert selbst, `setpoint_actual` zeigt dann denselben Wert wie der Sollwert.

Codes für `operation_mode`: 0 Auto, 1 Stopp, 2 Min, 3 Max, 4 Benutzerdefiniert, 7 Hand, 8 Standby.
Codes für `control_mode`: 0 Konstantdruck, 1 Prop. Druck, 2 Konstantdrehzahl, 8 Konstanter Förderstrom, 13 Prop. Druck (AutoAdapt), 14 Konstantdruck (AutoAdapt).

**Sprache:** Die `_text`-Sensoren, `venting_status` und die Auswahl der Regelart sind standardmäßig deutsch, angelehnt an die GO-App. `language: en` im sensor-Block schaltet sie auf Englisch. Die Sprache von Home Assistant lässt sich nicht übernehmen: Die ESPHome-API teilt dem Gerät die HA-Sprache nicht mit, und HA übersetzt Textzustände von ESPHome nicht.

## Bedienelemente

```yaml
switch:
  - platform: grundfos_alpha2go
    grundfos_alpha2go_id: pumpe   # id des sensor-Blocks; bei nur einer Pumpe weglassbar
    pump: {name: "Pumpe"}
    night_setback: {name: "Pumpe Nachtabsenkung"}
    air_detection: {name: "Pumpe Lufterkennung"}

select:
  - platform: grundfos_alpha2go
    control_mode: {name: "Pumpe Regelart"}

number:
  - platform: grundfos_alpha2go
    setpoint_head: {name: "Pumpe Sollwert Förderhöhe", mode: box}   # Prop. Druck / Konstantdruck
    setpoint_flow: {name: "Pumpe Sollwert Förderstrom"}             # Konstanter Förderstrom
    setpoint_speed: {name: "Pumpe Sollwert Drehzahl"}               # Konstantdrehzahl

button:
  - platform: grundfos_alpha2go
    venting_start: {name: "Pumpe System-Entlüftung starten"}
    venting_stop: {name: "Pumpe System-Entlüftung stoppen"}
```

| Entität | Wirkung |
|---|---|
| `pump` | Ein = Betriebsart Auto, Aus = Stopp |
| `night_setback` | Nachtabsenkung (AutoNight) ein/aus |
| `air_detection` | Kontinuierliche Lufterkennung ein/aus |
| `control_mode` | Prop. Druck, Konstantdruck, beide mit AutoAdapt, Konstanter Förderstrom, Konstantdrehzahl |
| `setpoint_head` / `setpoint_flow` / `setpoint_speed` | Sollwert in m / m³/h / 1/min; verstellbar ist nur der zur aktiven Regelart |
| `venting_start` / `venting_stop` | System-Entlüftung manuell starten / abbrechen |

- Der angezeigte Zustand kommt immer aus der Pumpe: Nach jedem Schreiben liest die Komponente ihn zurück. Änderungen in der App oder an der Pumpe erscheinen mit der nächsten Abfrage. Beim Booten schreibt der ESP nichts.
- Voreingestellte Grenzen: 0,5–6 m, 0,1–3,5 m³/h, 1000–5500 1/min. Anpassbar mit `min_value`, `max_value` und `step`.
- Wechselt die Regelart die Einheit, wird der zuletzt bekannte Sollwert dieser Einheit mitgeschickt, sonst 2 m, 1 m³/h bzw. 3000 1/min.
- Bei AutoAdapt wird der Sollwert angezeigt, lässt sich aber nicht verstellen.
- Lehnt die Pumpe einen Befehl ab, steht `rejected by pump` im Log, und die Entität springt auf den tatsächlichen Zustand zurück.
- Die Befehle ändern die lokale Anforderung der Pumpe, genau wie die GO-App. Die App kann danach alles wieder ändern.

## Verbindungsabbruch

Nach einer Trennung stehen alle Sensoren, die Auswahl und die Zahlen in Home Assistant auf „unbekannt“. Schalter behalten den letzten Zustand, weil die ESPHome-Integration von Home Assistant das Merkmal „kein Zustand“ bei Schaltern nicht auswertet. Ob sie aktuell sind, zeigt `connected`.

Manchmal verliert der BLE-Stack des ESP nach einer Trennung den Bond aus dem Arbeitsspeicher, obwohl er noch im Flash steht; jeder neue Verbindungsversuch scheitert dann mit `reason=82`. Die Komponente erkennt das und startet den BLE-Stack neu (`Restarting BLE stack to reload bonds from NVS`, höchstens alle 30 s). Danach verbindet sich die Pumpe ohne Tastendruck. Der Neustart betrifft alle BLE-Funktionen dieses ESP: Verbindungen zu weiteren Pumpen sind für etwa 8 s getrennt und kommen von selbst wieder.

## Fehlersuche

```yaml
logger:
  level: VERBOSE
  logs:
    grundfos_alpha2go: VERBOSE
    esp32_ble: INFO
    esp32_ble_client: INFO
    ble_client: INFO
```

Auf `DEBUG` loggt die Komponente Betriebsstatus und Einstellungen dekodiert, Timeouts mit Objektnummer und jede Trennung mit Grund (0x08 Funk-Timeout, 0x13 von der Pumpe getrennt, 0x16 vom ESP getrennt). `VERBOSE` zeigt zusätzlich alle Rohfelder jeder Antwort.

Für eigene Versuche sendet `id(pumpe).send_raw_class10({0x03, 0x5A, 0x00, 0x08});` eine beliebige GENI-Class-10-APDU, zum Beispiel aus einem Template-Button. Die Antwort wird geprüft und geloggt.

Protokolldetails (Objektaufbau, Schreibformat) stehen in [`docs/protocol.md`](docs/protocol.md) (Englisch).

## Tests

```bash
g++ -std=c++17 -I components/grundfos_alpha2go tests/test_geni.cpp -o t && ./t
```

## Offen

- „Nur Pumpe entlüften“ lässt sich noch nicht starten; die Funktion ist nur als Steuerquelle 111 in Objekt 86/8 sichtbar.
- Ungeprüft: andere Firmware-Stände und andere ALPHA2-GO-Varianten.

Lizenz: MIT. Unabhängiges Projekt, nicht von Grundfos.
