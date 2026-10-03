// Seite "Fahrt & Tank": Fahrt, Tankfüllung, Kosten, Knopf Getankt (Etappe 5)
// Etappe 1: Platzhalter.
#include "page.h"

namespace {
class TripTankPage : public PlaceholderPage {
 public:
  TripTankPage() : PlaceholderPage("Fahrt & Tank", 5) {}
};
}  // namespace

Page* tripTankPage() {
  static TripTankPage page;
  return &page;
}
