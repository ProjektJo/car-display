// Host-Stub: LittleFS als Ordner auf dem PC ($FSDIR, sonst /tmp/claude-0/host/fs).
// Bleibt zwischen zwei Läufen erhalten, damit Neustarts mit gespeicherten Daten prüfbar sind.
#pragma once
#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>
#include <string>

class File {
 public:
  File() = default;
  explicit File(FILE* f) : f_(f) {}
  explicit operator bool() const { return f_ != nullptr; }
  size_t read(uint8_t* buf, size_t size) { return f_ ? fread(buf, 1, size, f_) : 0; }
  size_t write(const uint8_t* buf, size_t size) { return f_ ? fwrite(buf, 1, size, f_) : 0; }
  bool seek(size_t pos) { return f_ && fseek(f_, (long)pos, SEEK_SET) == 0; }
  void close() {
    if (f_) fclose(f_);
    f_ = nullptr;
  }

 private:
  FILE* f_ = nullptr;
};

class HostLittleFS {
 public:
  bool begin(bool, const char*, uint8_t, const char*) {
    root_ = getenv("FSDIR") ? getenv("FSDIR") : "/tmp/claude-0/host/fs";
    ::mkdir(root_.c_str(), 0755);
    return true;
  }
  bool exists(const char* p) {
    struct stat st;
    return ::stat(full(p).c_str(), &st) == 0;
  }
  bool mkdir(const char* p) { return ::mkdir(full(p).c_str(), 0755) == 0; }
  File open(const char* p, const char* mode) {
    const std::string m = std::string(mode) == "r" ? "rb" : std::string(mode) == "w" ? "wb" : "r+b";
    return File(fopen(full(p).c_str(), m.c_str()));
  }
  size_t usedBytes() { return 0; }
  size_t totalBytes() { return 1408 * 1024; }

 private:
  std::string full(const char* p) { return root_ + p; }
  std::string root_;
};
inline HostLittleFS LittleFS;
