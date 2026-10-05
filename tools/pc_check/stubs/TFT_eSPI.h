#pragma once
#include <cstdint>
#define TFT_BLACK 0x0000
#define TFT_LIGHTGREY 0xD69A
#define TL_DATUM 0
#define MC_DATUM 4
class TFT_eSPI {
 public:
  void init();
  void setRotation(uint8_t r);
  void setSwapBytes(bool s);
  void fillScreen(uint32_t c);
  void setTextColor(uint16_t, uint16_t) {}
  void setTextDatum(uint8_t) {}
  void setTextSize(uint8_t) {}
  int16_t drawString(const char*, int32_t, int32_t) { return 0; }
  bool initDMA(bool ctrl_cs = false);
  void startWrite();
  void endWrite();
  void dmaWait();
  void setAddrWindow(int32_t x, int32_t y, int32_t w, int32_t h);
  void pushPixels(const void* data, uint32_t len);
  void pushPixelsDMA(uint16_t* data, uint32_t len);
};
