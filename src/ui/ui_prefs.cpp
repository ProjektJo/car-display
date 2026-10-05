#include "ui_prefs.h"

#include "storage/storage_task.h"
#include "storage/store.h"

namespace uiprefs {

namespace {
UiSettings settings;
}

void load() { store::loadUi(settings); }

UiSettings& get() { return settings; }

void save() { storage::saveUi(settings); }

}  // namespace uiprefs
