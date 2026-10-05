// Reichweite überall gleich (U Seite 4, A7): geteilter Balken gefahren seit Tanken (muted) | Rest
// (accent, unter 50 km warn), darunter links "⛽ ··· 316" (km seit Tanken) und rechts "Σ 763 km"
// (gefahren + Reichweite).
#pragma once
#include <lvgl.h>

#include "core/car_state.h"

class RangeBar {
 public:
  // width in px; bigText = größere Zeile (Großanzeige)
  void create(lv_obj_t* parent, int32_t width, bool bigText = false);
  void update(const CarSnapshot& s);
  lv_obj_t* obj() const { return root_; }

 private:
  lv_obj_t* root_ = nullptr;
  lv_obj_t* driven_ = nullptr;
  lv_obj_t* rest_ = nullptr;
  lv_obj_t* left_ = nullptr;
  lv_obj_t* right_ = nullptr;
  int32_t width_ = 0;
  int32_t shownDrivenW_ = -1;
  uint32_t shownColor_ = 0;
  char shownLeft_[24] = "";
  char shownRight_[24] = "";
};
