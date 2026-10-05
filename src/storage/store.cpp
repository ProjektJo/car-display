#include "store.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>

#include <cstddef>
#include <cstring>

#include "config.h"
#include "storage/profile_json.h"
#include "util/crc32.h"

namespace store {

namespace {

#ifdef SIMULATE_OBD
constexpr const char* ROOT = "/sim";
constexpr const char* NVS_NAMESPACE = "cardisp_sim";
#else
constexpr const char* ROOT = "";
constexpr const char* NVS_NAMESPACE = "cardisp";
#endif
constexpr const char* NVS_LAST_PROFILE = "last_prof";
constexpr size_t JSON_MAX = 1536;

// Kopf der Ringdateien (Fahrtenbuch, Tankfüllungen)
struct RingHeader {
  uint32_t magic;
  uint16_t recSize;
  uint16_t capacity;
  uint16_t count;
  uint16_t next;
};
constexpr uint32_t RING_MAGIC = 0x52474E52;  // "RNGR"

bool mounted = false;

void path(char* out, size_t size, const char* fmt, uint8_t id) {
  char rel[40];
  snprintf(rel, sizeof(rel), fmt, id);
  snprintf(out, size, "%s%s", ROOT, rel);
}

void ensureDir(const char* rel) {
  char p[32];
  snprintf(p, sizeof(p), "%s%s", ROOT, rel);
  if (!LittleFS.exists(p)) LittleFS.mkdir(p);
}

bool readFile(const char* p, void* buf, size_t size, size_t& got) {
  got = 0;
  if (!LittleFS.exists(p)) return false;
  File f = LittleFS.open(p, "r");
  if (!f) return false;
  got = f.read(static_cast<uint8_t*>(buf), size);
  f.close();
  return true;
}

bool writeFile(const char* p, const void* buf, size_t size) {
  File f = LittleFS.open(p, "w");
  if (!f) return false;
  const size_t n = f.write(static_cast<const uint8_t*>(buf), size);
  f.close();  // LittleFS übernimmt die Datei erst hier; ein Stromausfall davor lässt die alte Version stehen
  return n == size;
}

bool validState(const PersistState& st, uint8_t id) {
  return st.magic == PERSIST_MAGIC && st.version == PERSIST_VERSION && st.profileId == id &&
         st.crc == crc32(&st, offsetof(PersistState, crc));
}

bool appendRing(const char* p, const void* rec, uint16_t recSize, uint16_t capacity) {
  RingHeader h{};
  bool fresh = true;
  if (LittleFS.exists(p)) {
    File f = LittleFS.open(p, "r");
    if (f && f.read(reinterpret_cast<uint8_t*>(&h), sizeof(h)) == sizeof(h) && h.magic == RING_MAGIC &&
        h.recSize == recSize && h.capacity == capacity && h.next < capacity)
      fresh = false;
    if (f) f.close();
  }
  if (fresh) h = RingHeader{RING_MAGIC, recSize, capacity, 0, 0};

  File f = LittleFS.open(p, fresh ? "w" : "r+");
  if (!f) return false;
  bool ok = f.seek(sizeof(RingHeader) + static_cast<size_t>(h.next) * recSize);
  ok = ok && f.write(static_cast<const uint8_t*>(rec), recSize) == recSize;
  h.next = static_cast<uint16_t>((h.next + 1) % capacity);
  if (h.count < capacity) h.count++;
  ok = ok && f.seek(0) && f.write(reinterpret_cast<const uint8_t*>(&h), sizeof(h)) == sizeof(h);
  f.close();
  return ok;
}

}  // namespace

bool begin() {
  // Partition "spiffs" aus partitions.csv; formatieren nur, wenn sie sich nicht einbinden lässt
  mounted = LittleFS.begin(true, "/littlefs", 10, "spiffs");
  if (!mounted) {
    Serial.println("Speicher: LittleFS nicht nutzbar, es wird nichts gespeichert");
    return false;
  }
  if (ROOT[0] && !LittleFS.exists(ROOT)) LittleFS.mkdir(ROOT);
  ensureDir("/profiles");
  ensureDir("/data");
  Serial.printf("Speicher: LittleFS %u von %u kB belegt\n", (unsigned)(LittleFS.usedBytes() / 1024),
                (unsigned)(LittleFS.totalBytes() / 1024));
  return true;
}

int listProfiles(ProfileSummary* out, int max) {
  int n = 0;
  for (uint8_t id = 1; id <= cfg::MAX_PROFILES && n < max; id++) {
    Profile p;
    if (loadProfile(id, p)) out[n++] = ProfileSummary::of(p);
  }
  return n;
}

bool loadProfile(uint8_t id, Profile& p) {
  if (!mounted || id == 0) return false;
  char pth[48];
  path(pth, sizeof(pth), "/profiles/p%u.json", id);
  static char json[JSON_MAX];
  size_t got = 0;
  if (!readFile(pth, json, sizeof(json) - 1, got) || got == 0) return false;
  json[got] = '\0';
  p.id = id;
  return profileFromJson(json, p);
}

bool saveProfile(Profile& p) {
  if (!mounted) return false;
  if (p.id == 0) {
    for (uint8_t id = 1; id <= cfg::MAX_PROFILES; id++) {
      char pth[48];
      path(pth, sizeof(pth), "/profiles/p%u.json", id);
      if (!LittleFS.exists(pth)) {
        p.id = id;
        break;
      }
    }
    if (p.id == 0) return false;  // alle Plätze belegt
  }
  static char json[JSON_MAX];
  const size_t len = profileToJson(p, json, sizeof(json));
  if (len == 0) return false;
  char pth[48];
  path(pth, sizeof(pth), "/profiles/p%u.json", p.id);
  return writeFile(pth, json, len);
}

bool loadState(uint8_t id, PersistState& st) {
  if (!mounted) return false;
  static PersistState other;
  char a[48], b[48];
  path(a, sizeof(a), "/data/p%u_a.bin", id);
  path(b, sizeof(b), "/data/p%u_b.bin", id);
  size_t got = 0;
  const bool okA = readFile(a, &st, sizeof(st), got) && got == sizeof(st) && validState(st, id);
  const bool okB = readFile(b, &other, sizeof(other), got) && got == sizeof(other) && validState(other, id);
  if (okB && (!okA || other.seq > st.seq)) st = other;
  return okA || okB;
}

bool saveState(const PersistState& st) {
  if (!mounted) return false;
  static PersistState copy;
  copy = st;
  copy.crc = crc32(&copy, offsetof(PersistState, crc));
  // Abwechselnd A und B: bricht ein Schreiben ab, bleibt die andere Datei gültig
  char pth[48];
  path(pth, sizeof(pth), (copy.seq & 1) ? "/data/p%u_a.bin" : "/data/p%u_b.bin", copy.profileId);
  return writeFile(pth, &copy, sizeof(copy));
}

bool appendTrip(uint8_t id, const trip::TripRecord& r) {
  if (!mounted) return false;
  char pth[48];
  path(pth, sizeof(pth), "/data/p%u_trips.bin", id);
  return appendRing(pth, &r, sizeof(r), cfg::TRIP_LOG_SIZE);
}

bool appendFill(uint8_t id, const trip::FillRecord& r) {
  if (!mounted) return false;
  char pth[48];
  path(pth, sizeof(pth), "/data/p%u_fills.bin", id);
  return appendRing(pth, &r, sizeof(r), cfg::FILL_LOG_SIZE);
}

uint8_t lastProfileId() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return 0;
  const uint8_t id = prefs.getUChar(NVS_LAST_PROFILE, 0);
  prefs.end();
  return id;
}

void setLastProfileId(uint8_t id) {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) return;
  if (prefs.getUChar(NVS_LAST_PROFILE, 0) != id) prefs.putUChar(NVS_LAST_PROFILE, id);
  prefs.end();
}

}  // namespace store
