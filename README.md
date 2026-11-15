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
| `night_setback_enabled` | binär | 90/8 @0 | aus (wie in der App) |
| `night_mode` | binär | 90/7 @0 (gerade Nachtbetrieb) | aus |
| `air_detection_enabled` | binär | 88/600 @7 | an (wie in der App) |
| `air_detections` | Anzahl | 88/604 @1 (Lufterkennungen nach 168 Betriebsstunden) | 0 |
| `venting_active` / `venting_status` | binär / Text | 88/612 @1 und 86/8 @0 | „Inaktiv“, „Nur Pumpe“ (an HK1 geprüft), „Anlage und Pumpe (manuell/automatisch)“ |

Die Einstellungen (90/8, 88/600, 88/604) werden nur bei jeder 6. Abfrage gelesen (bei 10 s also etwa jede Minute) und direkt nach dem Verbinden. Nachtbetrieb und Entlüftung werden bei jeder Abfrage gelesen.

`energy` ist der Zähler der Pumpe selbst (wie „Kumulierte Leistungsaufnahme“ in der App), kein aus der Leistung integrierter Wert.

**Sollwert:** Die Pumpe liefert ihn in SI-Einheiten, abhängig von der Regelart. Weil ein Sensor in Home Assistant eine feste Einheit braucht, gibt es getrennte Sensoren:
`setpoint_head` gilt bei Druck-Regelarten (Konstant-/Proportionaldruck, Konstant-/Proportional-Differenzdruck, AutoAdapt), `setpoint_flow` bei Konstantem Durchfluss und FlowAdapt, `setpoint_speed` bei Konstantdrehzahl. Die jeweils anderen stehen dann auf „unbekannt“ (NaN).
`setpoint_actual` ist der momentane Zielwert, den die Pumpe gerade ausregelt: bei Proportionaldruck der Punkt auf der Kurve beim aktuellen Durchfluss, er liegt nah an der tatsächlichen Förderhöhe.
Verifiziert sind Prop. Druck (Pa), beide AutoAdapt-Varianten (Pa), Konstanter Förderstrom (m³/s) und Konstantdrehzahl (1/min); bei Konstantdruck ohne AutoAdapt ist Pa angenommen.
In der GO-App wird AutoAdapt innerhalb von „Prop. Druck“ oder „Konstantdruck“ eingeschaltet. Die Pumpe meldet das als eigene Regelart: 13 = Prop. Druck mit AutoAdapt (an HK1: 41 138 Pa = 4,19 m, tatsächlich 3,2 m), 14 = Konstantdruck mit AutoAdapt (29 412 Pa = 3,00 m, tatsächlich 2,99 m). Die Profilnamen „Radiator“/„Underfloor“ beziehen sich auf diese beiden Kennlinientypen.
Bei AutoAdapt meldet 86/8 denselben Wert wie 86/7; `setpoint_actual` zeigt dort also den Sollwert, nicht den momentanen Punkt auf der Kurve.

**Stopp** (Betriebsart 1): 86/6 und 86/7 behalten Regelart und Sollwert der letzten Einstellung, 86/8 meldet Konstantdrehzahl mit 0. Durchfluss, Förderhöhe, Leistung und Drehzahl gehen auf 0; im Betriebsverlauf (93/1) zählt die Betriebszeit nicht weiter, die Einschaltdauer schon.

Codes für `operation_mode`: 0 = Auto, 1 = Stopp, 2 = Min, 3 = Max, 4 = Benutzerdefiniert, 7 = Hand, 8 = Standby.
Codes für `control_mode`: 0 = Konstantdruck, 1 = Prop. Druck, 2 = Konstantdrehzahl, 8 = Konstanter Förderstrom, 13 = Prop. Druck (AutoAdapt), 14 = Konstantdruck (AutoAdapt); weitere Codes aus dem Profil (5, 10, 15 …) bietet die GO-App für die ALPHA2 GO nicht an.
Die `_text`-Sensoren liefern dasselbe als deutschen Klartext, angelehnt an die Begriffe der GO-App. Im Log stehen zum Vergleich auch 86/6 (lokale Anforderung) und 86/8.

Es wird nur angefragt, was konfiguriert ist. Die Antworten werden über den Objekttyp erkannt, nicht über die Version. Dadurch spielt es keine Rolle, ob die Pumpe Typ 256 als v1, v2 oder v3 meldet.
Bei einem Verbindungsabbruch gehen alle Sensoren auf `NaN`.

## Einrichtung

1. `example/grundfos-alpha2go.yaml` anpassen (`pump_mac`, WLAN).
2. Flashen. **Beim ersten Verbinden die Bluetooth-Taste an der Pumpe drücken**, sonst `auth fail reason=82`. Der Bond liegt danach im Flash des ESP32 und übersteht Neustarts und OTA-Updates: An der Testpumpe hat sich der ESP nach mehreren OTA-Updates jedes Mal ohne erneuten Tastendruck wieder verbunden.
   **Ausnahme:** Wird die Verbindung getrennt (ESP oder GO-App) und direkt danach neu aufgebaut, lehnt die Pumpe die Verschlüsselung oft mit `reason=82` ab, obwohl der Bond besteht. Der ESP versucht es automatisch erneut; ein Druck auf die Bluetooth-Taste behebt es sofort. Ohne Tastendruck genügt es, etwa eine Minute zu warten (an HK1 geprüft); kürzere Pausen sind noch nicht getestet.
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

**Einstellungs-Objekte** (an der ALPHA2 GO geprüft; Werte an HK1: 88/600 = 50 / 1000 / 150 / 5 / aktiviert, passend zur App)

| Objekt | Typ | Inhalt |
|---|---|---|
| 90/8 | 333 `AutoNightEnable` | @0 bool: Nachtabsenkung aktiviert |
| 90/7 | 324 `AutoNightStatus` | @0 bool: gerade Nachtbetrieb |
| 88/600 | 600 `AirDetectionConfiguration`, 8 Byte | @0 u16 Drehzahländerung, @2 u16 Druckänderung, @4 u16 Empfindlichkeit, @6 u8 Ignorierzeit nach Start, @7 bool aktiviert |
| 88/604 | 984 `AirDetectCounterStorage`, 2 Byte | @0 u8 Erkennungen in den ersten 24 Betriebsstunden, @1 u8 nach 168 Betriebsstunden |
| 88/612 | 971 `SystemAirVentingCommandAndStatus`, 5 Byte | @0 Befehl (0 Stopp, 1 Start manuell), @1 Status (0 inaktiv, 1 manuell, 2 automatisch), @2 u16 Fortschritt, @4 Phase (0 Idle, 1–4 Phase 0–3) |

„Nur Pumpe entlüften“ (10 min) erscheint **nicht** in 88/612, sondern in 86/8 als Steuerquelle 111 (AirVenting) mit Konstantdrehzahl 5225 1/min; 86/6 und 86/7 bleiben unverändert. 88/612 gehört zu „Entlüftung von Anlage und Pumpe“. `venting_active`/`venting_status` werten beides aus.

## Fehlersuche

Mit `logger` auf `VERBOSE` für `grundfos_alpha2go` loggt die Komponente jede Antwort mit Typ, Version und Länge, dazu alle Rohfelder (Floats von 565 und 256, Hex-Dump von 248 und 534, dekodierte Werte von 232). Für 87/1, 86/6–8, 90/7–8 und 88/600–612 erscheinen zusätzlich die rohen Notifications (`raw RX`), also auch Antworten, die der Parser verwirft. 86/6–8, 90/7–8 und 88/600–612 werden schon auf `DEBUG` dekodiert geloggt, Timeouts mit Objektnummer (`Reply timeout (90/8)`).

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

## Experimentell: Schreibtests (nur Branch `feature/write-access`)

Das Format der Schreibbefehle ist noch nicht an der Pumpe bestätigt. Die Komponente sendet nur, wenn eine der Testmethoden ausdrücklich aufgerufen wird; Anfrage (`Test write TX`) und Antwort (`Test write RX`) werden auf `WARN` roh geloggt, danach liest sie Betriebsstatus (86/6–8), Elektrik (87/69) und Nachtabsenkung (90/8) sofort zurück.

| Methode | Telegramm | Status |
|---|---|---|
| `test_command(5)` / `(6)` / `(7)` / `(8)` | Class 3 STOP / START·AUTO / REMOTE / LOCAL: `27 05 E7 F8 03 81 <Befehl> CRC` (klassisches GENIbus-Format) | ✅ STOP und START an HK1 bestätigt (Antwort `24 04 F8 E7 03 00 CRC` = Quittung OK); LOCAL quittiert, ohne sichtbare Wirkung; REMOTE nicht getestet |
| `test_set_night_setback(true/false, true)` | Class-10-SET 90/8, Variante A (mit Objektkopf) | ungetestet |
| `test_set_night_setback(true/false, false)` | Class-10-SET 90/8, Variante B (nur Adresse + Daten) | ungetestet |

STOP/START ändern die **lokale** Anforderung (86/6: Betriebsart Stopp/Auto, `control_source` bleibt Panel) – wie eine Änderung in der GO-App; Regelart und Sollwert bleiben erhalten. Die App kann den Zustand danach normal wieder ändern.

`test_command` nimmt nur die Befehle 5–8 an (kein RESET, FACTORY_BOOT o. ä.). Rückgängig machen geht jederzeit in der GO-App (ESP vorher trennen).

Einbindung zum Testen (die Komponente braucht eine `id`):

```yaml
external_components:
  - source: github://CDank/esphome-grundfos-alpha2go-component@feature/write-access
    components: [grundfos_alpha2go]
    refresh: 0s

sensor:
  - platform: grundfos_alpha2go
    id: pumpe_go
    ble_client_id: pumpe
    # ... Sensoren wie gewohnt ...

button:
  - platform: template
    name: "Pumpe TEST Stopp"
    on_press: [lambda: "id(pumpe_go).test_command(5);"]
  - platform: template
    name: "Pumpe TEST Start (Auto)"
    on_press: [lambda: "id(pumpe_go).test_command(6);"]
  - platform: template
    name: "Pumpe TEST Lokal"
    on_press: [lambda: "id(pumpe_go).test_command(8);"]
  - platform: template
    name: "Pumpe TEST Nachtabsenkung EIN (A)"
    on_press: [lambda: "id(pumpe_go).test_set_night_setback(true, true);"]
  - platform: template
    name: "Pumpe TEST Nachtabsenkung AUS (A)"
    on_press: [lambda: "id(pumpe_go).test_set_night_setback(false, true);"]
  - platform: template
    name: "Pumpe TEST Nachtabsenkung EIN (B)"
    on_press: [lambda: "id(pumpe_go).test_set_night_setback(true, false);"]
  - platform: template
    name: "Pumpe TEST Nachtabsenkung AUS (B)"
    on_press: [lambda: "id(pumpe_go).test_set_night_setback(false, false);"]
```

## Offen

- **Schreiben** (Sollwert, Regelart) ist nicht implementiert. Laut Profil sind 86/6 und 86/10 (Typ 303) beschreibbar, aber das Format der SET-Anfrage steht nicht in der App-APK (nur im fehlenden Dart-Code). Dafür wird ein Bluetooth-Mitschnitt der GO-App beim Ändern des Sollwerts gebraucht.
- Ungeprüft: andere Firmware-Stände, andere ALPHA2-GO-Varianten.

Lizenz: MIT. Unabhängiges Projekt, nicht von Grundfos.
