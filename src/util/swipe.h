// Gesten-Erkennung für die Seitennavigation: Wischen links/rechts und von oben nach unten.
// Schwellen wie in der Vorschau (config.h). Reines C++, nativ testbar.
#pragma once
#include <cstdint>
#include <cstdlib>

enum class Swipe : uint8_t { None, Left, Right, Down };

class SwipeDetector {
 public:
  SwipeDetector(int16_t tapSlop, int16_t minDx, int16_t downMinDy, int16_t downStartMaxY)
      : tapSlop_(tapSlop), minDx_(minDx), downMinDy_(downMinDy), downStartMaxY_(downStartMaxY) {}

  // Einmal je Touch-Abfrage aufrufen. Liefert beim Loslassen die erkannte Geste.
  Swipe update(bool pressed, int16_t x, int16_t y) {
    if (pressed) {
      if (!down_) {
        down_ = true;
        moved_ = false;
        x0_ = x;
        y0_ = y;
      }
      lastX_ = x;
      lastY_ = y;
      const int dx = x - x0_, dy = y - y0_;
      if (dx * dx + dy * dy > tapSlop_ * tapSlop_) moved_ = true;
      return Swipe::None;
    }
    if (!down_) return Swipe::None;
    down_ = false;
    const int dx = lastX_ - x0_, dy = lastY_ - y0_;
    // Waagerecht: mindestens minDx und mehr waagerecht als senkrecht. Finger nach links = nächste Seite.
    if (std::abs(dx) > minDx_ && std::abs(dx) > std::abs(dy)) return dx < 0 ? Swipe::Left : Swipe::Right;
    if (dy > downMinDy_ && y0_ < downStartMaxY_) return Swipe::Down;
    return Swipe::None;
  }

  // true, solange der Finger unten ist und sich weiter als tapSlop bewegt hat
  bool movedWhileDown() const { return down_ && moved_; }

 private:
  int16_t tapSlop_, minDx_, downMinDy_, downStartMaxY_;
  bool down_ = false, moved_ = false;
  int16_t x0_ = 0, y0_ = 0, lastX_ = 0, lastY_ = 0;
};
