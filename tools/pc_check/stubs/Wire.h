#pragma once
#include <cstdint>
#include <cstddef>
class TwoWire {
 public:
  bool begin(int sda, int scl, uint32_t freq);
  void beginTransmission(uint8_t addr);
  size_t write(uint8_t b);
  uint8_t endTransmission(bool stop = true);
  uint8_t requestFrom(uint8_t addr, uint8_t len);
  int read();
};
extern TwoWire Wire;
