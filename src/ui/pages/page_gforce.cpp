// Seite "G-Kraft": Kreis mit Quer- und Längsbeschleunigung, nur mit MPU6050 (Etappe 8)
// Etappe 1: Platzhalter, ausgeblendet solange kein Sensor erkannt ist.
#include "page.h"

namespace {
class GForcePage : public PlaceholderPage {
 public:
  GForcePage() : PlaceholderPage("G-Kraft", 8) {}
  bool available(const CarSnapshot& s) const override { return s.hasImu; }
};
}  // namespace

Page* gforcePage() {
  static GForcePage page;
  return &page;
}
