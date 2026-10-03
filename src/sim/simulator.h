// Simulierte Fahrt für die Umgebung "simulator" (M, Abschnitt Simulator; A13).
//
// Liefert Rohwerte, wie sie ein Auto über OBD meldet: Kaltstart, Stadt, Landstraße bis
// 100 km/h, Schub, Ampel-Leerlauf, Segeln, ausgekuppeltes Bremsen, ein Fehlercode (P0171),
// ein Tankvorgang mit Motor aus und eine Vollgas-Sequenz aus dem Stand mit Schaltpausen.
// Das Fahrzeug entspricht dem Renault Modus 1.2 (Saugrohrdruck statt MAF, kein 0x5E).
// Der Saugrohrdruck ist so gewählt, dass die Speed-Density-Rechnung (A7) den
// gewünschten Verbrauch ergibt.
//
// Reines C++ ohne Arduino: Zeit kommt als Schrittweite herein, nativ testbar.
#pragma once
#include <cstdint>

struct SimOutput {
  bool engineOn = true;
  float speedKmh = 0;      // 0x0D
  float rpm = 0;           // 0x0C
  float mapKpa = 0;        // 0x0B
  float throttlePct = 0;   // 0x11
  float pedalPct = 0;      // 0x49
  float iatC = 0;          // 0x0F
  float coolantC = 0;      // 0x05
  float stftPct = 0;       // 0x06
  float ltftPct = 0;       // 0x07
  float loadPct = 0;       // 0x04
  uint8_t fuelSys = 2;     // 0x03: 1 = offen (kalt), 2 = geregelt, 4 = Schubabschaltung
  float fuelLevelPct = 0;  // 0x2F (in 100/255-%-Schritten wie vom Auto)
  float voltage = 0;       // ATRV
  bool mil = false;        // 0x01
  uint8_t dtcCount = 0;    // 0x01
  uint16_t dtc = 0;        // gespeicherter Code, 0x0171 = P0171

  // Zur Kontrolle (kommt nicht über OBD): wahrer Verbrauch und Gang der Simulation
  float trueLph = 0;
  uint8_t gear = 0;        // 0 = Leerlauf/ausgekuppelt
};

class DriveSim {
 public:
  enum class Mode : uint8_t {
    Idle,        // Stand, Motor im Leerlauf
    Accel,       // normales Beschleunigen
    Cruise,      // konstant bzw. leicht ändernd, im Gang
    Overrun,     // Schub: im Gang rollen, Gas weg, Schubabschaltung
    Sail,        // Segeln: ausgekuppelt rollen, Tempo hält sich fast
    CoastBrake,  // ausgekuppelt bremsen (Anlass für den Tipp "Gang rein")
    EngineOff,   // Motor aus (Tanken)
    Sprint,      // Vollgas aus dem Stand mit Schaltpausen
  };

  explicit DriveSim(uint32_t seed = 1);

  // Schritt um dt Sekunden weiter
  void step(float dtS);
  // Vollgas-Sequenz beim nächsten Halt starten (BOOT-Taste lang im Simulator)
  void requestSprint() { sprintRequested_ = true; }

  const SimOutput& out() const { return out_; }
  float timeS() const { return t_; }
  Mode mode() const { return mode_; }

  // Fahrzeugdaten der Simulation (Renault Modus 1.2, A6/A7)
  static constexpr float TANK_L = 49.0f;
  static constexpr float DISPLACEMENT_L = 1.149f;
  static constexpr float VE = 0.85f;
  static constexpr float AFR = 14.7f;
  static constexpr float DENSITY_G_PER_L = 745.0f;

 private:
  float rnd(float amplitude);   // gleichverteilt in [-amplitude, +amplitude]
  void startSegment(int index);
  void stepDriving(float dtS);
  void stepEngineOff(float dtS);
  float mapForFuel(float lph, float rpm) const;

  uint32_t rng_;
  float t_ = 0;                 // Simulationszeit s
  int seg_ = 0;                 // aktueller Abschnitt des Fahrzyklus
  float segT_ = 0;              // Zeit im Abschnitt
  float segV0_ = 0;             // Tempo am Abschnittsbeginn
  Mode mode_ = Mode::Idle;
  float speed_ = 0;             // km/h ohne Rauschen
  float tankL_ = 28.6f;         // Liter im Tank
  float coolant_ = 15.0f;       // Kaltstart
  float ltft_ = 1.5f;
  bool dtcSet_ = false;
  bool refuelled_ = false;
  float offT_ = 0;              // Zeit mit Motor aus
  bool sprintRequested_ = false;
  float sprintT_ = -1;          // Zeit seit Sprintbeginn, < 0 = kein Sprint
  float shiftPauseUntil_ = -1;  // Ende der aktuellen Schaltpause (Sprintzeit)
  uint8_t sprintGear_ = 1;
  float nextAutoSprint_;
  SimOutput out_;
};
