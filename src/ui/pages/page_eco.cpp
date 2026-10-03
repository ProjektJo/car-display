// Seite "Eco": Startseite: Momentanverbrauch, Eco-Kurve, Gang, Hinweise (Etappe 4)
// Etappe 1: Platzhalter.
#include "page.h"

namespace {
class EcoPage : public PlaceholderPage {
 public:
  EcoPage() : PlaceholderPage("Eco", 4) {}
};
}  // namespace

Page* ecoPage() {
  static EcoPage page;
  return &page;
}
