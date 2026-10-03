// Seite "Sport": Drehzahlbogen, Live-Balken, 30-s-Diagramm (Etappe 6)
// Etappe 1: Platzhalter.
#include "page.h"

namespace {
class SportPage : public PlaceholderPage {
 public:
  SportPage() : PlaceholderPage("Sport", 6) {}
};
}  // namespace

Page* sportPage() {
  static SportPage page;
  return &page;
}
