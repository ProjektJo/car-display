// Seite "Diagramme": Verläufe über 1, 5 oder 30 min (Etappe 5)
// Etappe 1: Platzhalter.
#include "page.h"

namespace {
class ChartsPage : public PlaceholderPage {
 public:
  ChartsPage() : PlaceholderPage("Diagramme", 5) {}
};
}  // namespace

Page* chartsPage() {
  static ChartsPage page;
  return &page;
}
