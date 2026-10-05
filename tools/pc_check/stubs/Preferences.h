// Host-Stub: NVS als Dateien im LittleFS-Ordner des PCs (nvs_<namespace>_<key>)
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

class Preferences {
 public:
  bool begin(const char* ns, bool) {
    ns_ = ns;
    return true;
  }
  void end() {}
  uint8_t getUChar(const char* key, uint8_t def) {
    FILE* f = fopen(path(key).c_str(), "rb");
    if (!f) return def;
    int v = fgetc(f);
    fclose(f);
    return v < 0 ? def : static_cast<uint8_t>(v);
  }
  size_t putUChar(const char* key, uint8_t v) {
    FILE* f = fopen(path(key).c_str(), "wb");
    if (!f) return 0;
    fputc(v, f);
    fclose(f);
    return 1;
  }

 private:
  std::string path(const char* key) {
    return std::string(getenv("FSDIR") ? getenv("FSDIR") : "/tmp/claude-0/host/fs") + "/nvs_" + ns_ + "_" + key;
  }
  std::string ns_;
};
