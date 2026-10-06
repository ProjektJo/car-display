#include "dtc.h"

#include <cstdio>

namespace dtc {

// Tabelle in dtc_table.cpp (erzeugt aus data/dtc_de.csv mit tools/make_dtc_table.py), nach Code sortiert
struct Entry {
  Code code;
  const char* text;
};
extern const Entry TABLE[];
extern const int TABLE_SIZE;

namespace {
constexpr const char* MANUFACTURER = "Herstellerspezifischer Code";

void add(Code c, Code* out, int& count, int max) {
  if (c == 0 || count >= max) return;
  for (int i = 0; i < count; i++)
    if (out[i] == c) return;
  out[count++] = c;
}
}  // namespace

int decode(const elmp::Message* msgs, int n, uint8_t mode, Code* out, int max) {
  int count = 0;
  const uint8_t ok = static_cast<uint8_t>(0x40 + mode);
  for (int m = 0; m < n; m++) {
    const elmp::Message& msg = msgs[m];
    if (msg.len < 1 || msg.data[0] != ok) continue;
    int start = 1;
    // CAN: zweites Byte = Anzahl, danach genau so viele Paare
    if (msg.len >= 2 && (msg.len - 2) % 2 == 0 && msg.data[1] == (msg.len - 2) / 2) start = 2;
    for (int i = start; i + 1 < msg.len; i += 2) add(static_cast<Code>((msg.data[i] << 8) | msg.data[i + 1]), out, count, max);
  }
  return count;
}

void format(Code c, char* out, size_t size) {
  static const char LETTERS[] = "PCBU";
  snprintf(out, size, "%c%X%X%X%X", LETTERS[c >> 14], (c >> 12) & 0x3, (c >> 8) & 0xF, (c >> 4) & 0xF, c & 0xF);
}

const char* text(Code c) {
  int lo = 0, hi = TABLE_SIZE - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    if (TABLE[mid].code == c) return TABLE[mid].text;
    if (TABLE[mid].code < c) lo = mid + 1;
    else hi = mid - 1;
  }
  return MANUFACTURER;
}

int tableSize() { return TABLE_SIZE; }

}  // namespace dtc
