#include "format.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace fmt {

namespace {
constexpr int MAX_DECIMALS = 3;
constexpr int64_t POW10[MAX_DECIMALS + 1] = {1, 10, 100, 1000};
constexpr int GROUP = 3;  // Tausendergruppen
constexpr float MAX_ABS = 1e12f;  // größere Zahlen sind Unsinn (z. B. falsch gelesene Antwort)

size_t copyOut(char* out, size_t size, const char* src, size_t len) {
  if (size == 0) return 0;
  size_t n = len < size - 1 ? len : size - 1;
  memcpy(out, src, n);
  out[n] = '\0';
  return n;
}
}  // namespace

size_t number(char* out, size_t size, float v, int decimals) {
  if (std::isnan(v) || std::isinf(v) || std::fabs(v) > MAX_ABS) return copyOut(out, size, NO_VALUE, strlen(NO_VALUE));
  if (decimals < 0) decimals = 0;
  if (decimals > MAX_DECIMALS) decimals = MAX_DECIMALS;

  // Kaufmännisch runden (weg von der Null), dann ganzzahlig weiterarbeiten.
  // Ein float trägt nur ca. 7 Stellen: 14,15 ist intern 14,1499996. Ein Zuschlag von einem
  // float-Schritt (ulp) sorgt dafür, dass solche Werte wie geschrieben gerundet werden (14,15 -> 14,2).
  const double ulp = std::fabs(static_cast<double>(std::nextafter(v, v >= 0 ? INFINITY : -INFINITY)) - v);
  const double x = (static_cast<double>(v) + (v >= 0 ? ulp : -ulp)) * static_cast<double>(POW10[decimals]);
  const int64_t scaled = llround(x);
  const bool negative = scaled < 0;  // -0,04 ergibt 0 und damit kein Minus
  const uint64_t mag = static_cast<uint64_t>(negative ? -scaled : scaled);
  uint64_t intPart = mag / static_cast<uint64_t>(POW10[decimals]);
  uint64_t frac = mag % static_cast<uint64_t>(POW10[decimals]);

  // Von hinten aufbauen
  char tmp[40];
  int pos = sizeof(tmp);
  for (int i = 0; i < decimals; i++) {
    tmp[--pos] = static_cast<char>('0' + frac % 10);
    frac /= 10;
  }
  if (decimals > 0) tmp[--pos] = ',';
  int digits = 0;
  do {
    if (digits > 0 && digits % GROUP == 0) tmp[--pos] = '.';
    tmp[--pos] = static_cast<char>('0' + intPart % 10);
    intPart /= 10;
    digits++;
  } while (intPart > 0);
  if (negative) tmp[--pos] = '-';
  return copyOut(out, size, tmp + pos, sizeof(tmp) - pos);
}

}  // namespace fmt
