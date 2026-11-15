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

Ein vollständiges Beispiel mit allen Sensoren und einem kWh-Zähler aus der Leistung steht in [`example/grundfos-alpha2go.yaml`](example/grundfos-alpha2go.yaml).

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
| @12 (1 Byte) | vermutlich steady_state-Flag (erst `00`, dann `01`) | unbelegt |

## Fehlersuche

Mit `logger` auf `VERBOSE` für `grundfos_alpha2go` loggt die Komponente jede Antwort mit Typ, Version und Länge, dazu alle Rohfelder (Floats von 565 und 256, Hex-Dump von 248 und 534):

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

- **Energiezähler der Pumpe** (kWh, wie in der App): Objekt 87/1, Feldlayout unbekannt. Die Option `energy:` ist bisher **nur Diagnose**: Sie fragt 87/1 ab und loggt Anfrage und Rohantwort auf `DEBUG` (`87/1 request`, `87/1 raw RX`, `Unknown reply type …`), liefert aber noch keinen Wert. Bis das Layout geklärt ist, ersetzt ein `integration`-Sensor aus `power` den Zähler (siehe Beispiel-YAML).
- **Schreiben** (Sollwert, Regelart) ist nicht implementiert; das SET-Format ist nicht belegt.
- Ungeprüft: Bond nach ESP-Neustart, andere Firmware-Stände, andere ALPHA2-GO-Varianten.

Lizenz: MIT. Unabhängiges Projekt, nicht von Grundfos.
