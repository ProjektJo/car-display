// Sprintmessung und Auto-Sprint (A10 Sport und Sprint): 0–50, 0–100 und 80–120 km/h mit Start-,
// Gültigkeits- und Abbruchregeln, Tempoverlauf der letzten und der besten Messung, Erkennung eines
// Sprints aus dem Stand und der automatische Wechsel zur Sprint-Seite und zurück.
// Reines C++ ohne Arduino und LVGL: Zeit und Messwerte kommen als Parameter, nativ testbar.
#pragma once
#include <cmath>
#include <cstdint>

#include "config.h"

namespace perf {

// Tempoverlauf einer Messung 0–100: Zeit in 1/100 s, zu der 0, 5, 10 … 100 km/h erreicht wurden
constexpr int TRACE_POINTS = 21;
constexpr float TRACE_STEP_KMH = 5.0f;
struct Trace {
  uint16_t cs[TRACE_POINTS];  // 0 = (noch) nicht erreicht, außer beim ersten Punkt
  uint8_t count;              // erreichte Punkte
  void clear();
  void add(float tS, float vKmh);  // nach jeder Messung aufrufen; trägt überschrittene Stufen ein
  float timeAt(int i) const { return cs[i] / 100.0f; }
};

// Beste Zeiten eines Profils (in der Speicherdatei)
struct Best {
  float s50;      // NAN = noch keine
  float s100;
  float s80120;
  Trace trace100; // Verlauf der besten 0–100-Messung
  void clear();
};

enum class State : uint8_t {
  Ready,     // bereit: steht bzw. wartet auf das nächste Anfahren aus dem Stand
  Waiting,   // rollt aus dem Stand, Live-Kurve läuft, (noch) kein Sprint: zählt nicht
  Running,   // Sprint erkannt, 0–100 läuft und zählt
  Done,      // Ziel erreicht
};

// Letztes Ergebnis (für das Urteil auf der Sport-Seite)
enum class Kind : uint8_t { None, S50, S100, S80120 };

class SprintMeter {
 public:
  SprintMeter() { best_.clear(); }  // noch keine Bestzeiten (NAN)
  void reset();
  void setBest(const Best& b) { best_ = b; }
  const Best& best() const { return best_; }

  // Je neuer Tempo-Messung (Zeitpunkt der Messung): Tempo, Gaspedal relativ zum gelernten Bereich
  // (0 = leer, 100 = Vollgas; NAN = unbekannt) und Beschleunigung m/s² (gefiltert, NAN = unbekannt)
  void update(uint32_t tMs, float speedKmh, float pedalRelPct, float accelMs2);

  State state() const { return state_; }
  // Sprint erkannt (Vollgas aus dem Stand): zählt bei jedem neuen Sprint hoch (Auto-Sprint)
  uint16_t launchSeq() const { return launchSeq_; }
  // Messung aktiv (Scheduler fragt dann nur Tempo, Drehzahl und Gas ab); im Stand bereit
  bool active() const { return state_ == State::Waiting || state_ == State::Running || run80_; }
  bool standing() const { return standing_; }
  bool run80() const { return run80_; }
  // Laufende Zeit 0–100 in s (Running), sonst NAN
  float elapsed(uint32_t nowMs) const;
  // Letzte Ergebnisse (NAN = keins)
  float last50() const { return last50_; }
  float last100() const { return last100_; }
  float last80120() const { return last80120_; }
  uint32_t doneAtMs() const { return doneAt_; }
  // Verlauf der laufenden (auch Live ohne Sprint) bzw. letzten gültigen Messung
  const Trace& lastTrace() const { return state_ == State::Running || state_ == State::Waiting ? cur_ : trace_; }
  // Jedes neue Ergebnis zählt resultSeq hoch; dazu Art, Zeit und die Bestzeit davor (NAN = keine)
  uint16_t resultSeq() const { return resultSeq_; }
  Kind resultKind() const { return resultKind_; }
  float resultS() const { return resultS_; }
  float resultPrevBest() const { return resultPrevBest_; }
  // true einmal nach einer neuen Bestzeit (zum Speichern)
  bool takeBestChanged();

 private:
  void abort();
  void result(Kind k, float s, float& best);

  State state_ = State::Ready;
  Best best_ = {};
  bool bestChanged_ = false;
  // Anfahren erkennen (A10 Auto-Sprint)
  uint32_t standSince_ = 0;    // steht seit
  uint32_t leftAt_ = 0;        // Zeitpunkt des Anfahrens nach ≥ 1 s Stand, 0 = keiner
  bool launchLatched_ = false; // Sprint dieses Anfahrens schon gezählt
  uint16_t launchSeq_ = 0;
  // Messung 0–x
  uint32_t startMs_ = 0;
  uint32_t prevT_ = 0;
  float prevV_ = NAN;
  float vTop_ = 0;
  bool standing_ = true;
  float t50_ = NAN;            // laufende Messung
  uint32_t doneAt_ = 0;
  Trace trace_ = {};           // letzte gültige Messung
  Trace cur_ = {};             // laufende Aufzeichnung
  float last50Run_ = NAN;      // 0–50 dieser Messung schon gezählt
  float last50_ = NAN, last100_ = NAN, last80120_ = NAN;
  // Messung 80–120
  bool run80_ = false;
  uint32_t start80_ = 0;
  float vTop80_ = 0;
  // letztes Ergebnis
  uint16_t resultSeq_ = 0;
  Kind resultKind_ = Kind::None;
  float resultS_ = NAN, resultPrevBest_ = NAN;
};

// Auto-Sprint (A10): wechselt bei einem erkannten Sprint zur Sprint-Seite und kehrt zurück
// 4 s nach dem Ziel, 2 s nachdem das Gas unter 50 % fällt, wenn 3 s nach dem Wechsel keine Messung läuft
// oder spätestens nach 25 s. Wechselt der Fahrer selbst die Seite oder öffnet ein Fenster, entfällt der Rücksprung.
class AutoSprint {
 public:
  enum class Action : uint8_t { None, ShowSprint, Return };

  // enabled = Menü "Auto-Sprint"; overlayOpen = ein Fenster ist offen
  Action update(uint32_t nowMs, bool enabled, uint16_t launchSeq, State st, uint32_t doneAtMs, float pedalPct,
                bool overlayOpen);
  // Fahrer hat selbst gewischt bzw. ein Fenster geöffnet
  void cancel() { switched_ = false; }
  bool switched() const { return switched_; }

 private:
  uint16_t seenLaunch_ = 0;
  bool init_ = false;
  bool switched_ = false;
  uint32_t switchedAt_ = 0;
  uint32_t lowSince_ = 0;
  bool wasRunning_ = false;
};

}  // namespace perf
