// CRC-32 (IEEE 802.3, wie zlib) für die Prüfsumme der Speicherdateien (A8 "A/B mit Prüfsumme").
// Reines C++, nativ testbar.
#pragma once
#include <cstddef>
#include <cstdint>

inline uint32_t crc32(const void* data, size_t len, uint32_t crc = 0) {
  const uint8_t* p = static_cast<const uint8_t*>(data);
  crc = ~crc;
  for (size_t i = 0; i < len; i++) {
    crc ^= p[i];
    for (int b = 0; b < 8; b++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}
