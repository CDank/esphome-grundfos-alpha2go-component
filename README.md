# ESPHome-Komponente: Grundfos ALPHA2 GO (BLE, nur lesend)

Externe ESPHome-Komponente `grundfos_alpha2go` für die **ALPHA2 GO**. Die in ESPHome enthaltene `alpha3`-Komponente passt nicht:
Sie fragt für Flow/Head ein Objekt ab (93/289), das nur die ursprüngliche ALPHA3 hat, und erwartet die ALPHA3-Antwortkennungen.

**Status:** Auf einer ALPHA2 GO getestet, Lesen funktioniert. Alle 10 s kommen vier Antworten ohne Timeouts oder CRC-Fehler, alle Werte erscheinen in Home Assistant.
Kompiliert mit ESPHome 2026.10.2 (ESP-IDF und Arduino). Die Firmware-Version der Testpumpe ist nicht bestimmt.
Das Protokoll stammt aus der Grundfos-GO-APK und dem Mitschnitt von `Lasse-source/esphome-grundfos-alpha2-go`. Die Verbindungsreihenfolge ist daraus übernommen.

## Einbindung

```yaml
external_components:
  - source: github://CDank/esphome-grundfos-alpha2go-component@main
    components: [grundfos_alpha2go]

esp32_ble_tracker:

ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF"   # MAC der Pumpe (Name im Scan: "ALPHA2 GO")
    id: pumpe

sensor:
  - platform: grundfos_alpha2go
    ble_client_id: pumpe
    flow: {name: "Pumpe Durchfluss"}
    head: {name: "Pumpe Förderhöhe"}
    power: {name: "Pumpe Leistung"}
    media_temperature: {name: "Pumpe Medientemperatur"}
```

Ein vollständiges Beispiel mit allen Sensoren steht in [`example/grundfos-alpha2go.yaml`](example/grundfos-alpha2go.yaml).

## Sensoren

Messwerte von der Testpumpe (Heizkreis, Dauerbetrieb):

| Option | Einheit | Objekt / Feld | Beispielwert |
|---|---|---|---|
| `flow` | m³/h | 93/290 @24 | 1,69 |
| `head` | m | 93/290 @28 | 2,11 |
| `voltage` | V | 87/69 @0 | 223 |
| `dc_voltage` | V | 87/69 @4 | 309 |
| `current` | A | 87/69 @8 | 0,164 |
| `power` | W | 87/69 @12 | 21,6 |
| `speed` | 1/min | 87/69 @20 | 3452 |
| `aux_temperature` | °C | 87/69 @32 (Elektronik) | 62 |
| `starts` | – | 93/1 @0 | 158 |
| `operating_hours` | h | 93/1 @8 | 4858 |
| `power_on_hours` | h | 93/1 @24 | 4858 |
| `media_temperature` | °C | 93/300 @0 | 38,0 |
| `pcb_temperature` | °C | 93/300 @4 | 59,6 |
| `controlbox_temperature` | °C | 93/300 @8 | 50,5 |
| `energy` | kWh | 87/1 @0 (J ÷ 3,6·10⁶) | 77,47 |
| `operation_mode` / `operation_mode_text` | Code / Text | 86/7 @1 | 0 / „Auto“ |
| `control_mode` / `control_mode_text` | Code / Text | 86/7 @2 | 1 / „Proportionaldruck“ |
| `setpoint_head` | m | 86/7 @3 (Pa ÷ 9806,65) | 2,50 (bei Proportionaldruck) |
| `setpoint_flow` | m³/h | 86/7 @3 (m³/s × 3600) | 1,00 (bei Konstantem Durchfluss) |
| `setpoint_speed` | 1/min | 86/7 @3 | 4060 (bei Konstantdrehzahl) |
| `setpoint_actual` | m | 86/8 @3 (Pa ÷ 9806,65) | 2,09–2,11 (Punkt auf der Kurve) |
| `setpoint` | SI-Rohwert | 86/7 @3 | 24 509,8 Pa / 0,000278 m³/s |

`energy` ist der Zähler der Pumpe selbst (wie „Kumulierte Leistungsaufnahme“ in der App), kein aus der Leistung integrierter Wert.

**Sollwert:** Die Pumpe liefert ihn in SI-Einheiten, abhängig von der Regelart. Weil ein Sensor in Home Assistant eine feste Einheit braucht, gibt es getrennte Sensoren:
`setpoint_head` gilt bei Druck-Regelarten (Konstant-/Proportionaldruck, Konstant-/Proportional-Differenzdruck, AutoAdapt), `setpoint_flow` bei Konstantem Durchfluss und FlowAdapt, `setpoint_speed` bei Konstantdrehzahl. Die jeweils anderen stehen dann auf „unbekannt“ (NaN).
`setpoint_actual` ist der momentane Zielwert, den die Pumpe gerade ausregelt: bei Proportionaldruck der Punkt auf der Kurve beim aktuellen Durchfluss, er liegt nah an der tatsächlichen Förderhöhe.
Verifiziert sind Proportionaldruck (Pa), AutoAdapt Heizkörper (Pa), Konstanter Durchfluss (m³/s) und Konstantdrehzahl (1/min); bei den übrigen Druck-Regelarten ist Pa angenommen.
Bei AutoAdapt meldet 86/8 denselben Wert wie 86/7 (an HK1: 41 138 Pa = 4,19 m bei 3,2 m tatsächlicher Förderhöhe); `setpoint_actual` zeigt dort also den Kurvensollwert, nicht den momentanen Punkt.

Codes für `operation_mode`: 0 = Auto, 1 = Stopp, 2 = Min, 3 = Max, 4 = Benutzerdefiniert, 7 = Hand, 8 = Standby.
Codes für `control_mode`: 0 = Konstantdruck, 1 = Proportionaldruck, 2 = Konstantdrehzahl, 5 = AutoAdapt, 8 = Konstanter Durchfluss, 10 = FlowAdapt, 13 = AutoAdapt Heizkörper, 14 = AutoAdapt Fußboden, 15 = AutoAdapt Heizkörper + Fußboden, 254 = keine.
Die `_text`-Sensoren liefern dasselbe als deutschen Klartext. Im Log stehen zum Vergleich auch 86/6 (lokale Anforderung) und 86/8.

Es wird nur angefragt, was konfiguriert ist. Die Antworten werden über den Objekttyp erkannt, nicht über die Version. Dadurch spielt es keine Rolle, ob die Pumpe Typ 256 als v1, v2 oder v3 meldet.
Bei einem Verbindungsabbruch gehen alle Sensoren auf `NaN`.

## Einrichtung

1. `example/grundfos-alpha2go.yaml` anpassen (`pump_mac`, WLAN).
2. Flashen. **Beim ersten Verbinden die Bluetooth-Taste an der Pumpe drücken**, sonst `auth fail reason=82`. Der Bond liegt danach im Flash des ESP32.
3. Die GO-App auf dem Handy schließen. Die Pumpe akzeptiert nur einen Client.

Ablauf pro Verbindung: Verschlüsselung → CCCD schreiben → Erstabfrage der App → Abfragen nacheinander (eine gleichzeitig, 30 ms Abstand, 1 s Timeout).

Mehrere Pumpen: je Pumpe ein eigener `ble_client` und ein eigener `grundfos_alpha2go`-Block; jede Pumpe einzeln per Bluetooth-Taste koppeln.

## Protokoll: Feldbelegung (an der Hardware geprüft)

Alle Werte Big Endian, Floats IEEE-754. `NaN` bedeutet: Die Pumpe liefert dieses Feld nicht.
Die Feldnamen stammen aus einem GENI-Geräteprofil, das in der Grundfos-GO-App als Testdatei enthalten ist (`test_geni_profile_30_4_1.xml`); die Objekte 87/1, 87/69, 93/1, 93/290 und 93/300 stimmen darin in ID und Typ mit der ALPHA2 GO überein.

**93/290, Typ 565 v2, 36 Byte**

| Offset | Inhalt | ALPHA2 GO |
|---|---|---|
| @0 | Durchfluss in m³/s | ✅ (×3600 = @24) |
| @4 | Förderdruck in Pa | ✅ (÷9806,65 = @28) |
| @8 … @20 | Einlass-/Auslassdruck, Medien-/Differenztemperatur | `NaN` |
| @24 | Durchfluss in m³/h | ✅ → `flow` |
| @28 | Förderhöhe in m | ✅ → `head` |
| @32 | kv-Wert | ✅ (nur Log) |

**87/69, Typ 256 v3, 41 Byte**

| Offset | Inhalt | ALPHA2 GO |
|---|---|---|
| @0 / @4 | Netzspannung / Zwischenkreisspannung | ✅ |
| @8 / @12 | Strom / Leistung | ✅ |
| @16 | DC-Leistung | ✅ (≈ @12, nur Log) |
| @20 | Drehzahl | ✅ |
| @24 / @28 | Umrichter-/Wechselrichtertemperatur | `NaN` |
| @32 | Elektroniktemperatur | ✅ |
| @36 | PWM | nicht ausgewertet |

**93/1, Typ 248 v2, 28 Byte**

| Offset | Inhalt | Beispiel |
|---|---|---|
| @0 u32 | Starts gesamt | 158 |
| @4 / @6 u16 | Starts letzte Stunde / letzte 24 h | 0 / 0 |
| @8 u32 | Betriebszeit gesamt (s) | 4858 h |
| @12 u32 | Betriebszeit letzte 24 h (s) | 23,6 h |
| @16 u32 | Laufzeit seit letztem Start (s) | 27,4 Tage |
| @20 u32 | Anzahl Einschaltvorgänge | 157 |
| @24 u32 | Einschaltdauer gesamt (s) | 4858 h |

Nur @0, @8 und @24 sind als Sensoren verfügbar, die übrigen Felder erscheinen im VERBOSE-Log.

**93/300, Typ 534 v2, 13 Byte** (nicht 16)

| Offset | Inhalt | ALPHA2 GO |
|---|---|---|
| @0 | Medientemperatur | ✅ |
| @4 | Platinentemperatur | ✅ |
| @8 | Controlbox-Temperatur | ✅ |
| @12 (1 Byte) | `steady_state` (bool) | ✅ |

**87/1, Typ 232 v1 `OverallEnergyConsumption`, 26 Byte (bei einer Pumpe)**

| Offset | Inhalt | ALPHA2 GO |
|---|---|---|
| @0 | Energie gesamt, double in J | ✅ 278 880 350 J = 77,47 kWh (+220 J pro 10 s bei 22 W) |
| @8 | Leistung gesamt, float in W | ✅ 22,04 |
| @12 | Anzahl Pumpen, u16 | ✅ 1 |
| @14 | je Pumpe: Leistung (float, W), Energie (double, J) | ✅ wie @8 / @0 |

**86/6, 86/7, 86/8, Typ 303 v1 `OperationStatusRequest`, 7 Byte**

| Offset | Inhalt | ALPHA2 GO |
|---|---|---|
| @0 | `control_source` (0 = nicht verwendet, 1 = Panel, 2 = Bus, 104 = AutoNight, …) | ✅ 86/6: 0, 86/7 und 86/8: 1 |
| @1 | `operation_mode` | ✅ 0 = Auto |
| @2 | `control_mode` | ✅ 1 = Proportionaldruck, 8 = Konstanter Durchfluss |
| @3 | `set_point`, float | ✅ Pa, m³/s bzw. 1/min je nach Regelart |

86/6 ist die lokale Anforderung (App/Taste), 86/7 die priorisierte Anforderung nach Bewertung von lokal/remote/Alarm, 86/8 die tatsächlich umgesetzte Anforderung.
Bei Proportionaldruck enthalten 86/6 und 86/7 den eingestellten Sollwert (24 509,8 Pa = 2,50 m), 86/8 den momentanen Punkt auf der Kurve (≈ 20 500 Pa = 2,09 m, schwankt mit dem Durchfluss). Bei Konstantem Durchfluss sind alle drei gleich.

## Fehlersuche

Mit `logger` auf `VERBOSE` für `grundfos_alpha2go` loggt die Komponente jede Antwort mit Typ, Version und Länge, dazu alle Rohfelder (Floats von 565 und 256, Hex-Dump von 248 und 534, dekodierte Werte von 232). Für 87/1 und 86/6–8 erscheinen zusätzlich die rohen Notifications (`raw RX`), also auch Antworten, die der Parser verwirft. 86/6–8 werden schon auf `DEBUG` dekodiert geloggt, Timeouts mit Objektnummer (`Reply timeout (86/7)`).

```yaml
logger:
  level: VERBOSE
  initial_level: DEBUG
  runtime_tag_levels: true
  logs:
    grundfos_alpha2go: VERBOSE
    esp32_ble: INFO
    esp32_ble_client: INFO
    ble_client: INFO
```

## Tests

PC-Unittests für CRC, Request-Frames, Antwortkennungen und die Reassemblierung zerstückelter Notifications. Die Flow/Head-Prüfung nutzt echte Werte aus dem Hardware-Log.

```bash
g++ -std=c++17 -I components/grundfos_alpha2go tests/test_geni.cpp -o t && ./t
```

Mit MSVC (Developer Command Prompt):

```bat
cl /std:c++17 /EHsc /I components\grundfos_alpha2go tests\test_geni.cpp /Fe:t.exe && t.exe
```

## Offen

- **Schreiben** (Sollwert, Regelart) ist nicht implementiert. Laut Profil sind 86/6 und 86/10 (Typ 303) beschreibbar, aber das Format der SET-Anfrage steht nicht in der App-APK (nur im fehlenden Dart-Code). Dafür wird ein Bluetooth-Mitschnitt der GO-App beim Ändern des Sollwerts gebraucht.
- Ungeprüft: andere Firmware-Stände, andere ALPHA2-GO-Varianten. (Der Bond übersteht Neustart und OTA-Update des ESP; an HK1 mehrfach ohne erneutes Drücken der Bluetooth-Taste verbunden.)

Lizenz: MIT. Unabhängiges Projekt, nicht von Grundfos.
