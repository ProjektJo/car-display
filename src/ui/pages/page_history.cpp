// Seite "Historie": Fahrten, Tankfüllungen, Auswertung, Spartempo (Etappe 7)
// Etappe 1: Platzhalter.
#include "page.h"

namespace {
class HistoryPage : public PlaceholderPage {
 public:
  HistoryPage() : PlaceholderPage("Historie", 7) {}
};
}  // namespace

Page* historyPage() {
  static HistoryPage page;
  return &page;
}
