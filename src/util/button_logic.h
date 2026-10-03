// Auswertung der BOOT-Taste: kurz / lang / sehr lang, entprellt.
// Reines C++ (Zeit und Pegel kommen als Parameter), nativ testbar.
#pragma once
#include <cstdint>

enum class ButtonEvent : uint8_t { None, Short, Long, VeryLong };

class ButtonLogic {
 public:
  // longMs: ab hier "lang". veryLongMs = 0: es gibt kein "sehr lang", dann meldet die Taste
  // "lang" sofort beim Erreichen der Zeit (noch während sie gedrückt ist).
  // veryLongMs > 0: "lang" kommt erst beim Loslassen (0,8 s bis veryLongMs),
  // "sehr lang" sofort beim Erreichen von veryLongMs.
  ButtonLogic(uint32_t debounceMs, uint32_t longMs, uint32_t veryLongMs)
      : debounceMs_(debounceMs), longMs_(longMs), veryLongMs_(veryLongMs) {}

  // pressed: entprellter Rohpegel (true = gedrückt). Einmal je Schleifendurchlauf aufrufen.
  ButtonEvent update(bool pressed, uint32_t nowMs) {
    // Entprellen: neuer Pegel gilt erst, wenn er debounceMs stabil anliegt
    if (pressed != raw_) {
      raw_ = pressed;
      rawSince_ = nowMs;
    }
    if (raw_ != stable_ && nowMs - rawSince_ >= debounceMs_) {
      stable_ = raw_;
      if (stable_) {
        downAt_ = nowMs;
        reported_ = false;
      } else if (!reported_) {
        const uint32_t held = nowMs - downAt_;
        reported_ = true;
        return held >= longMs_ ? ButtonEvent::Long : ButtonEvent::Short;
      }
    }
    if (stable_ && !reported_) {
      const uint32_t held = nowMs - downAt_;
      if (veryLongMs_ > 0 && held >= veryLongMs_) {
        reported_ = true;
        return ButtonEvent::VeryLong;
      }
      if (veryLongMs_ == 0 && held >= longMs_) {
        reported_ = true;
        return ButtonEvent::Long;
      }
    }
    return ButtonEvent::None;
  }

 private:
  uint32_t debounceMs_, longMs_, veryLongMs_;
  bool raw_ = false, stable_ = false, reported_ = true;
  uint32_t rawSince_ = 0, downAt_ = 0;
};
