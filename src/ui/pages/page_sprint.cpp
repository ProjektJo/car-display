// Seite "Sprint": Messungen 0–50, 0–100, 80–120, Auto-Sprint (Etappe 6)
// Etappe 1: Platzhalter.
#include "page.h"

namespace {
class SprintPage : public PlaceholderPage {
 public:
  SprintPage() : PlaceholderPage("Sprint", 6) {}
};
}  // namespace

Page* sprintPage() {
  static SprintPage page;
  return &page;
}
