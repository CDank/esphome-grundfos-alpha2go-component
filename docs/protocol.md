# ALPHA2 GO: GENI over BLE

Notes for developers. Everything here was checked on an ALPHA2 GO 25-60 180.

All values are big endian, floats are IEEE-754. `NaN` means the pump does not provide the field.
Field names come from a GENI device profile shipped as a test file in the Grundfos GO app (`test_geni_profile_30_4_1.xml`).

## Connection

Encryption (bonded) → write CCCD → the app's initial class 2 query (IDs 148/149/150, twice) → requests one at a time, 30 ms gap, 1 s timeout.
Replies are matched by object type, not version, so it doesn't matter whether the pump reports type 256 as v1, v2 or v3.

## Read objects

**93/290, type 565 v2, 36 bytes**

| Offset | Content | ALPHA2 GO |
|---|---|---|
| @0 | Flow, m³/s | ✅ |
| @4 | Pump pressure, Pa | ✅ |
| @8 … @20 | Inlet/outlet pressure, media/differential temperature | `NaN` |
| @24 | Flow, m³/h | ✅ → `flow` |
| @28 | Head, m | ✅ → `head` |
| @32 | kv value | ✅ (log only) |

**87/69, type 256 v3, 41 bytes**

| Offset | Content | ALPHA2 GO |
|---|---|---|
| @0 / @4 | Mains voltage / DC link voltage | ✅ |
| @8 / @12 | Current / power | ✅ |
| @16 | DC power | ✅ (≈ @12, log only) |
| @20 | Speed | ✅ |
| @24 / @28 | Converter / inverter temperature | `NaN` |
| @32 | Electronics temperature | ✅ → `aux_temperature` |
| @36 | PWM | not used |

**93/1, type 248 v2, 28 bytes**

| Offset | Content |
|---|---|
| @0 u32 | Starts total → `starts` |
| @4 / @6 u16 | Starts last hour / last 24 h |
| @8 u32 | Running time total, s → `operating_hours` |
| @12 u32 | Running time last 24 h, s |
| @16 u32 | Time since last start, s |
| @20 u32 | Power-on count |
| @24 u32 | Powered time total, s → `power_on_hours` |

**93/300, type 534 v2, 13 bytes** (not 16): @0 media temperature, @4 PCB temperature, @8 control box temperature, @12 u8 `steady_state`.

**87/1, type 232 v1 `OverallEnergyConsumption`, 26 bytes (one pump):** @0 energy total (double, J), @8 power total (float, W), @12 u16 number of pumps, @14 per pump: power (float) and energy (double).

**86/6, 86/7, 86/8, type 303 v1 `OperationStatusRequest`, 7 bytes**

| Offset | Content |
|---|---|
| @0 | `control_source` (0 unused, 1 panel, 2 bus, 104 AutoNight, 111 AirVenting …) |
| @1 | `operation_mode` (0 Auto, 1 Stop, 2 Min, 3 Max, 4 User defined, 7 Hand, 8 Standby) |
| @2 | `control_mode` (0 const. pressure, 1 prop. pressure, 2 const. speed, 8 const. flow, 13/14 prop./const. pressure AutoAdapt) |
| @3 | `set_point`, float: Pa in pressure modes, m³/s in constant flow, rpm in constant speed |

86/6 is the local request (app/button), 86/7 the prioritised request, 86/8 the request actually applied.
In proportional pressure mode 86/6 and 86/7 hold the setpoint, 86/8 the current point on the curve. In AutoAdapt modes the pump picks its own setpoint and 86/8 equals 86/7.
When stopped, 86/6 and 86/7 keep the last mode and setpoint; 86/8 reports constant speed 0.

86/9 (`temporary_operation_control_obj`) holds source 0, Auto, constant speed, 5225 rpm when idle: exactly the speed used by "vent pump only". Probably its preset; not written yet.

**Settings**

| Object | Type | Content |
|---|---|---|
| 90/8 | 333 `AutoNightEnable` | @0 bool: night setback enabled |
| 90/7 | 324 `AutoNightStatus` | @0 bool: night mode active |
| 88/600 | 600 `AirDetectionConfiguration`, 8 bytes | @0 u16 speed change, @2 u16 pressure change, @4 u16 sensitivity, @6 u8 ignore time after start, @7 bool enabled |
| 88/604 | 984 `AirDetectCounterStorage`, 2 bytes | @0 u8 detections in the first 24 h, @1 u8 after 168 h |
| 88/612 | 971 `SystemAirVentingCommandAndStatus`, 5 bytes | @0 command (0 stop, 1 start), @1 status (0 idle, 1 manual, 2 automatic), @2 u16 progress, @4 phase |

"Vent pump only" (10 min) does not show in 88/612 but in 86/8 as control source 111 at constant speed 5225 rpm. 88/612 is "vent system and pump".

## Writing

- **Class 3 commands:** `27 05 E7 F8 03 81 <cmd> CRC`, 5 = STOP, 6 = START. Reply `03 00` = OK. Commands 66/67 (night reduction) from the profile are not supported (ACK 10).
- **Class 10 SET:** `27 LE E7 F8 0A <0x80|n> <ID> <SubHi> <SubLo> <TypeHi> <TypeLo> <Ver> 00 00 <DataLen> <Data> CRC`, i.e. the object header from GET replies without the leading status byte. Reply `0A 01 <status>`: 00 OK, 04 object/type not found, 01 unknown operation.
- **Long frames:** one BLE write holds 20 bytes. Longer frames (86/6: 24 bytes, 88/600: 25 bytes, 88/612: 22 bytes) are sent in 20-byte chunks; the pump reassembles them.

| Entity | Telegram |
|---|---|
| `pump` | Class 3 START / STOP |
| `night_setback` | SET 90/8 |
| `air_detection` | SET 88/600 (all 8 bytes, only @7 changed) |
| `control_mode`, `setpoint_*` | SET 86/6 (source, mode, control mode, setpoint in SI units) |
| `venting_start` / `venting_stop` | SET 88/612 (1 / 0) |
