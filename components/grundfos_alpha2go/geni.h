#pragma once
// Hardwareunabhaengige GENI-Hilfen (Framing, CRC, Reassemblierung). Keine ESPHome-Abhaengigkeit -> am PC testbar.
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome {
namespace grundfos_alpha2go {
namespace geni {

static constexpr uint8_t SD_REQUEST = 0x27;
static constexpr uint8_t SD_REPLY = 0x24;
static constexpr uint8_t SD_ECHO = 0x30;
static constexpr uint8_t ADDR_PUMP = 0xE7;
static constexpr uint8_t ADDR_CLIENT = 0xF8;
static constexpr uint8_t ADDR_BROADCAST = 0xFF;
static constexpr uint8_t CLASS_MEASURED = 0x02;
static constexpr uint8_t CLASS_OBJECT = 0x0A;
static constexpr size_t MAX_FRAME = 260;

// Objekt-Typen (Antwort-Header, siehe Grundfos-GENI-Profil 52_x)
static constexpr uint16_t TYPE_MEDIA_TEMPERATURE = 534;   // Obj 93/300
static constexpr uint16_t TYPE_PROCESS_VALUES_EXT = 565;  // Obj 93/290 (Flow/Head)
static constexpr uint16_t TYPE_MOTOR_DETAILS = 256;       // Obj 87/69  (v1/v2/v3 -> gleiches Layout fuer Offset 0..32)
static constexpr uint16_t TYPE_OPERATION_HISTORY = 248;   // Obj 93/1   (Zaehler)
// Layouts aus dem GENI-Profil in der GO-App (test_geni_profile_30_4_1.xml), auf der ALPHA2 GO noch nicht geprueft:
static constexpr uint16_t TYPE_ENERGY = 232;            // Obj 87/1 OverallEnergyConsumption: double@0, float@8, u16@12, [float,double]...
static constexpr uint16_t TYPE_OPERATION_STATUS = 303;  // Obj 86/6,7,8 OperationStatusRequest: u8 src, u8 mode, u8 ctrl, float@3

// CRC-16 (Poly 0x1021, Init 0xFFFF, Ergebnis invertiert), Big Endian im Frame.
inline uint16_t crc16(const uint8_t *d, size_t n) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    crc ^= (uint16_t) (d[i] << 8);
    for (int k = 0; k < 8; k++)
      crc = (crc & 0x8000) ? (uint16_t) ((crc << 1) ^ 0x1021) : (uint16_t) (crc << 1);
  }
  return (uint16_t) ~crc;
}

inline size_t finish_frame(uint8_t *out) {
  // out[1] = LE ist gesetzt; CRC ueber Index 1 .. 1+LE
  size_t n = (size_t) out[1] + 1;
  uint16_t c = crc16(out + 1, n);
  out[1 + n] = (uint8_t) (c >> 8);
  out[2 + n] = (uint8_t) (c & 0xFF);
  return (size_t) out[1] + 4;
}

// Class-10-Objekt lesen: 27 07 E7 F8 0A 03 <ID> <SubHi> <SubLo> CRC  (11 Byte)
inline size_t build_object_get(uint8_t *out, uint8_t obj_id, uint16_t sub_id) {
  out[0] = SD_REQUEST;
  out[1] = 7;
  out[2] = ADDR_PUMP;
  out[3] = ADDR_CLIENT;
  out[4] = CLASS_OBJECT;
  out[5] = 0x03;  // OS=GET (0), APDU-Datenlaenge 3
  out[6] = obj_id;
  out[7] = (uint8_t) (sub_id >> 8);
  out[8] = (uint8_t) (sub_id & 0xFF);
  return finish_frame(out);
}

// Class-2-Werte lesen (Erstabfrage der App): 27 LE FF F8 02 <n> <id>... CRC
inline size_t build_class2_get(uint8_t *out, const uint8_t *ids, uint8_t n, uint8_t da = ADDR_BROADCAST) {
  out[0] = SD_REQUEST;
  out[1] = (uint8_t) (4 + n);
  out[2] = da;
  out[3] = ADDR_CLIENT;
  out[4] = CLASS_MEASURED;
  out[5] = n;
  for (uint8_t i = 0; i < n; i++) out[6 + i] = ids[i];
  return finish_frame(out);
}

inline uint32_t be_u32(const uint8_t *p) {
  return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) | ((uint32_t) p[2] << 8) | (uint32_t) p[3];
}
inline uint16_t be_u16(const uint8_t *p) { return (uint16_t) ((p[0] << 8) | p[1]); }
inline float be_float(const uint8_t *p) {
  uint32_t u = be_u32(p);
  float f;
  memcpy(&f, &u, 4);
  return f;
}
inline double be_double(const uint8_t *p) {
  uint64_t u = ((uint64_t) be_u32(p) << 32) | be_u32(p + 4);
  double v;
  memcpy(&v, &u, 8);
  return v;
}

struct Reply {
  uint16_t type;
  uint8_t version;
  const uint8_t *data;  // Nutzdaten, Zeiger in den Frame
  uint8_t len;
};

// Setzt GENI-Antworten aus beliebig zerstueckelten Notifications wieder zusammen.
// Antwort: SD LE DA SA | 0A <APDU-Len> 00 <TypHi> <TypLo> <Version> 00 00 <DatenLen> | Daten | CRC(2)
class Reassembler {
 public:
  // Fuettert Bytes ein; ruft cb(const Reply&) pro gueltigem Class-10-Frame. Rueckgabe: Anzahl CRC-Fehler.
  template<typename F> int feed(const uint8_t *d, size_t n, F cb) {
    int crc_errors = 0;
    if (n_ + n > sizeof(buf_)) n_ = 0;  // Ueberlauf -> verwerfen
    if (n > sizeof(buf_)) return 0;
    memcpy(buf_ + n_, d, n);
    n_ += n;
    while (n_ >= 2) {
      if (buf_[0] != SD_REPLY && buf_[0] != SD_ECHO) {
        drop_(1);
        continue;
      }
      size_t tot = (size_t) buf_[1] + 4;
      if (tot < 8 || tot > MAX_FRAME) {
        drop_(1);
        continue;
      }
      if (n_ < tot) break;
      if (crc16(buf_ + 1, tot - 3) != (uint16_t) ((buf_[tot - 2] << 8) | buf_[tot - 1])) {
        crc_errors++;
        drop_(tot);
        continue;
      }
      if (buf_[4] == CLASS_OBJECT && tot >= 15 && buf_[6] == 0x00) {
        uint8_t dl = buf_[12];
        if ((size_t) 13 + dl + 2 == tot) {
          Reply r{(uint16_t) ((buf_[7] << 8) | buf_[8]), buf_[9], buf_ + 13, dl};
          cb(r);
        }
      }
      drop_(tot);
    }
    return crc_errors;
  }
  void reset() { n_ = 0; }

 protected:
  void drop_(size_t k) {
    if (k >= n_) {
      n_ = 0;
      return;
    }
    memmove(buf_, buf_ + k, n_ - k);
    n_ -= k;
  }
  uint8_t buf_[2 * MAX_FRAME]{};
  size_t n_{0};
};

}  // namespace geni
}  // namespace grundfos_alpha2go
}  // namespace esphome
