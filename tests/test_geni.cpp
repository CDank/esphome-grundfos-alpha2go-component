// Test des hardwareunabhaengigen Parsers: g++ -std=c++17 -I../components/grundfos_alpha2go test_geni.cpp && ./a.out
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#include "geni.h"
using namespace esphome::grundfos_alpha2go::geni;

static std::vector<uint8_t> hex(const char *s) {
  std::vector<uint8_t> v;
  unsigned x;
  while (*s) {
    if (*s == ' ') { s++; continue; }
    sscanf(s, "%2x", &x); v.push_back((uint8_t) x); s += 2;
  }
  return v;
}
static std::vector<uint8_t> reply(uint16_t type, uint8_t ver, const std::vector<uint8_t> &data) {
  std::vector<uint8_t> f = {SD_REPLY, (uint8_t) (2 + 2 + 7 + data.size()), ADDR_CLIENT, ADDR_PUMP, CLASS_OBJECT,
                            (uint8_t) (7 + data.size()), 0x00, (uint8_t) (type >> 8), (uint8_t) type, ver, 0, 0,
                            (uint8_t) data.size()};
  f.insert(f.end(), data.begin(), data.end());
  uint16_t c = crc16(f.data() + 1, f.size() - 1);
  f.push_back(c >> 8); f.push_back(c & 0xFF);
  return f;
}
static std::vector<uint8_t> floats(std::initializer_list<float> l) {
  std::vector<uint8_t> v;
  for (float f : l) { uint32_t u; memcpy(&u, &f, 4); for (int i = 3; i >= 0; i--) v.push_back(u >> (8 * i)); }
  return v;
}
static std::vector<uint8_t> tobytes(const uint8_t *p, size_t n) { return std::vector<uint8_t>(p, p + n); }

int main() {
  uint8_t f[16];
  // Request-Frames: bytegleich zu ESPHome-alpha3 (ALPHA3-Objekte) und zum Mitschnitt Lasse-source (ALPHA2 GO)
  assert(tobytes(f, build_object_get(f, 93, 289)) == hex("27 07 e7 f8 0a 03 5d 01 21 52 1f"));
  assert(tobytes(f, build_object_get(f, 87, 69)) == hex("27 07 e7 f8 0a 03 57 00 45 8a cd"));
  assert(tobytes(f, build_object_get(f, 93, 290)) == hex("27 07 e7 f8 0a 03 5d 01 22 62 7c"));
  assert(tobytes(f, build_object_get(f, 93, 1)) == hex("27 07 e7 f8 0a 03 5d 00 01 45 4c"));
  const uint8_t ids[] = {148, 149, 150};
  assert(tobytes(f, build_class2_get(f, ids, 3)) == hex("27 07 ff f8 02 03 94 95 96 4f 91"));
  assert(build_object_get(f, 93, 300) == 11 && tobytes(f, 11) == hex("27 07 e7 f8 0a 03 5d 01 2c 83 b2"));

  // Antwort-Kennungen aus dem Mitschnitt: Header-Bytes 5..12
  auto fh = reply(565, 2, std::vector<uint8_t>(36));
  assert(std::vector<uint8_t>(fh.begin() + 5, fh.begin() + 13) == hex("2b 00 02 35 02 00 00 24"));
  auto eh = reply(256, 3, std::vector<uint8_t>(41));
  assert(std::vector<uint8_t>(eh.begin() + 5, eh.begin() + 13) == hex("30 00 01 00 03 00 00 29"));
  auto ch = reply(248, 2, std::vector<uint8_t>(28));
  assert(std::vector<uint8_t>(ch.begin() + 5, ch.begin() + 13) == hex("23 00 00 f8 02 00 00 1c"));

  // Reassemblierung mit zerstueckelten Notifications (7/20/1 Byte), Muell davor, kaputter Frame dazwischen
  auto flow = reply(565, 2, floats({1.53f / 3600.f, 42400.f, NAN, NAN, NAN, NAN, 1.53f, 4.24f, 2.4f}));
  auto elec = reply(256, 3, floats({235.2f, 324.0f, 0.169f, 37.88f, 38.f, 4542.f, NAN, NAN, 51.0f, 0.f}));
  std::vector<uint8_t> hist(28, 0);
  hist[3] = 6;                                   // starts = 6
  uint32_t op = 31510844; for (int i = 0; i < 4; i++) hist[8 + i] = op >> (24 - 8 * i);
  auto cnt = reply(248, 2, hist);
  auto bad = flow; bad[20] ^= 0xFF;              // CRC-Fehler
  std::vector<uint8_t> stream = {0x00, 0x11, 0x22};  // Muell
  for (auto *v : {&bad, &flow, &elec, &cnt}) stream.insert(stream.end(), v->begin(), v->end());

  for (size_t chunk : {1u, 7u, 20u, 64u}) {
    Reassembler r;
    int n_flow = 0, n_elec = 0, n_cnt = 0, errs = 0;
    float q = 0, h = 0, u = 0, p = 0, rpm = 0, aux = 0, starts = 0, hours = 0;
    for (size_t i = 0; i < stream.size(); i += chunk) {
      errs += r.feed(stream.data() + i, std::min(chunk, stream.size() - i), [&](const Reply &rp) {
        if (rp.type == TYPE_PROCESS_VALUES_EXT) { n_flow++; q = be_float(rp.data) * 3600.f; h = be_float(rp.data + 4) * 0.0001f; }
        if (rp.type == TYPE_MOTOR_DETAILS) { n_elec++; u = be_float(rp.data); p = be_float(rp.data + 12); rpm = be_float(rp.data + 20); aux = be_float(rp.data + 32); }
        if (rp.type == TYPE_OPERATION_HISTORY) { n_cnt++; starts = be_u32(rp.data); hours = be_u32(rp.data + 8) / 3600.f; }
      });
    }
    assert(errs == 1 && n_flow == 1 && n_elec == 1 && n_cnt == 1);
    assert(fabsf(q - 1.53f) < 1e-3 && fabsf(h - 4.24f) < 1e-3);
    assert(fabsf(u - 235.2f) < 1e-3 && fabsf(p - 37.88f) < 1e-3 && rpm == 4542.f && aux == 51.0f);
    assert(starts == 6 && fabsf(hours - 8752.97f) < 0.05f);
  }
  printf("alle Tests OK\n");
}
