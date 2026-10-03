// Seite "Info": Kurzanleitung am Gerät (Etappe 7)
// Etappe 1: Platzhalter.
#include "page.h"

namespace {
class InfoPage : public PlaceholderPage {
 public:
  InfoPage() : PlaceholderPage("Info", 7) {}
};
}  // namespace

Page* infoPage() {
  static InfoPage page;
  return &page;
}
