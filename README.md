# ESPHome-Komponente: Grundfos ALPHA2 GO (BLE, nur lesend)

Externe ESPHome-Komponente `grundfos_alpha2go` für die **ALPHA2 GO**. Die in ESPHome enthaltene `alpha3`-Komponente passt nicht:
Sie fragt für Flow/Head ein Objekt ab (93/289), das nur die ursprüngliche ALPHA3 hat, und erwartet die ALPHA3-Antwortkennungen.

**Status:** kompiliert mit ESPHome 2026.10.2 (ESP-IDF 5.5), Parser per PC-Test geprüft. **Auf Hardware noch nicht getestet.**
Das Protokoll stammt aus der Grundfos-GO-APK und dem Mitschnitt von `Lasse-source/esphome-grundfos-alpha2-go` (dort läuft ein YAML-Package hardwaregetestet). Die Verbindungsreihenfolge ist daraus übernommen.

## Sensoren

| Option | Einheit | Quelle |
|---|---|---|
| `flow`, `head` | m³/h, m | Objekt 93/290 (Schätzwerte der Pumpe) |
| `power`, `voltage`, `dc_voltage`, `current`, `speed`, `aux_temperature` | W, V, V, A, 1/min, °C | Objekt 87/69 |
| `starts`, `operating_hours`, `power_on_hours` | –, h, h | Objekt 93/1 |
| `media_temperature`, `pcb_temperature`, `controlbox_temperature` | °C | Objekt 93/300, **ungetestet**, evtl. `NaN` |

Es wird nur angefragt, was konfiguriert ist. Die Antworten werden über den Objekttyp erkannt, nicht über die Version. Dadurch spielt es keine Rolle, ob die Pumpe Typ 256 als v1, v2 oder v3 meldet.

## Einrichtung

1. `example/grundfos-alpha2go.yaml` anpassen (`pump_mac`, WLAN, `external_components`-Pfad).
2. Flashen. **Beim ersten Verbinden die Bluetooth-Taste an der Pumpe drücken**, sonst `auth fail reason=82`. Der Bond liegt danach im Flash des ESP32.
3. Die GO-App auf dem Handy schließen. Die Pumpe akzeptiert nur einen Client.

Ablauf pro Verbindung: Verschlüsselung → CCCD schreiben → Erstabfrage der App → Abfragen nacheinander (eine gleichzeitig, 30 ms Abstand, 1 s Timeout).

## Tests

`cd tests && g++ -std=c++17 -I../components/grundfos_alpha2go test_geni.cpp -o t && ./t`
Prüft CRC und Request-Frames gegen die Referenzen (ESPHome-`alpha3`, Mitschnitt), die Antwortkennungen und die Reassemblierung zerstückelter Notifications.

## Offen

- Schreiben (Sollwert, Regelart) ist nicht implementiert; das SET-Format ist nicht belegt.
- Flow/Head kommen direkt aus @24/@28 (m³/h, m), wie von der Pumpe geliefert. Nur bei kürzeren Antworten wird aus @0 (m³/s) und @4 (Pa) gerechnet.
- Ungeprüft: Bond nach ESP-Neustart, andere Firmware, andere ALPHA2-GO-Varianten.

Lizenz: MIT. Unabhängiges Projekt, nicht von Grundfos.
