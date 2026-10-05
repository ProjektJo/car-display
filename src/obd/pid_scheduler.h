// PID-Scheduler (A7): Takt-Klassen schnell, mittel, langsam und selten.
// Reines C++ ohne Arduino, nativ testbar. Die Zeit kommt als Parameter herein.
//
// Eine Runde besteht aus allen schnellen PIDs und den Einträgen der anderen Klassen, die gerade
// fällig sind. Auf CAN gehen bis zu 6 PIDs in eine Anfrage, auf KWP und den älteren
// Protokollen immer nur einer. Nicht unterstützte PIDs werden nie abgefragt.
#pragma once
#include <cstdint>

enum class PidClass : uint8_t { Fast, Medium, Slow, Rare };

struct ObdRequest {
  uint8_t pids[8] = {};
  uint8_t count = 0;     // Zahl der PIDs (Mode 01)
  bool voltage = false;  // statt PIDs: ATRV (Bordspannung)
};

class PidScheduler {
 public:
  // supported: Bitfeld Mode 01 (Bit pid%8 in Byte pid/8). can = mehrere PIDs je Anfrage erlaubt.
  void reset(const uint8_t supported[32], bool can, uint8_t maxPerRequest);

  // Während einer Sprintmessung nur Tempo, Drehzahl und Gaspedal (A10, M Scheduler)
  void setSprint(bool on) {
    if (on == sprint_) return;
    sprint_ = on;
    roundPos_ = roundLen_;  // sofort mit einer passenden Runde weitermachen
  }
  bool sprint() const { return sprint_; }

  // Nächste Anfrage. Liefert immer etwas, solange überhaupt ein PID unterstützt wird
  // (sonst ATRV als einzige Abfrage).
  ObdRequest next(uint32_t nowMs);

  // true, wenn die laufende Runde ganz abgearbeitet ist
  bool roundFinished() const { return roundPos_ >= roundLen_; }

  // Für Tests und Diagnose: welche PIDs abgefragt werden und in welcher Klasse
  int itemCount() const { return itemCount_; }
  bool schedules(uint8_t pid) const;
  bool classOf(uint8_t pid, PidClass& cls) const;

 private:
  struct Item {
    uint8_t pid;        // 0 = ATRV
    PidClass cls;
    bool sprint;        // auch während der Sprintmessung
    uint32_t lastMs;    // zuletzt in eine Runde aufgenommen
    bool never;         // noch nie abgefragt
  };
  static constexpr int MAX_ITEMS = 24;
  static constexpr uint8_t VOLTAGE = 0;

  void add(uint8_t pid, PidClass cls, bool sprint);
  bool supported(uint8_t pid) const { return (sup_[pid / 8] >> (pid % 8)) & 1; }
  void buildRound(uint32_t nowMs);

  uint8_t sup_[32] = {};
  bool can_ = false;
  uint8_t maxPer_ = 1;
  bool sprint_ = false;
  Item items_[MAX_ITEMS] = {};
  int itemCount_ = 0;
  uint8_t round_[MAX_ITEMS] = {};  // Indizes in items_ für die laufende Runde
  int roundLen_ = 0;
  int roundPos_ = 0;
};
