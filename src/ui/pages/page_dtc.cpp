// Seite "Fehlercodes": Fehlercodes mit Klartext, Löschen im Stand (Etappe 7)
// Etappe 1: Platzhalter.
#include "page.h"

namespace {
class DtcPage : public PlaceholderPage {
 public:
  DtcPage() : PlaceholderPage("Fehlercodes", 7) {}
};
}  // namespace

Page* dtcPage() {
  static DtcPage page;
  return &page;
}
