// Host-Stub für Testkompilierung und Bildschirmfotos (nicht Teil der Firmware)
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cmath>
#include "freertos/FreeRTOS.h"
#include "esp_heap_caps.h"
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
uint32_t millis();
void pinMode(int, int);
void digitalWrite(int, int);
int digitalRead(int);
double ledcSetup(uint8_t ch, uint32_t freq, uint8_t bits);
void ledcAttachPin(uint8_t pin, uint8_t ch);
void ledcWrite(uint8_t ch, uint32_t duty);
struct HostSerial {
  void begin(unsigned long) {}
  int printf(const char* f, ...) __attribute__((format(printf, 2, 3))) { va_list a; va_start(a, f); int n = vprintf(f, a); va_end(a); return n; }
  void print(const char* s) { fputs(s, stdout); }
  void println(const char* s) { puts(s); }
  void println() { puts(""); }
};
extern HostSerial Serial;
