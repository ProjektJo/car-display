// Mittelwerte über Strecke (A7): echte Strecken-Ringpuffer statt gleitender Näherungen.
// Jeder Abschnitt speichert gefahrene Meter und verbrauchte Milliliter, der Schnitt ist
// Summe Liter ÷ Summe km. Nur der angebrochene Abschnitt läuft live mit.
// Die Strukturen sind POD, damit sie unverändert in die Speicherdatei (A/B) passen.
// Reines C++, nativ testbar.
#pragma once
#include <cmath>
#include <cstdint>

namespace avg {

constexpr uint16_t RING_MAX = 100;

struct DistanceRing {
  float m[RING_MAX];    // gefahrene Meter je Abschnitt
  float ml[RING_MAX];   // verbrauchte Milliliter je Abschnitt
  uint16_t head;        // laufender (angebrochener) Abschnitt
  uint16_t slots;       // Anzahl Abschnitte (20 bzw. 100)
  float slotM;          // Länge eines Abschnitts in m

  void init(uint16_t slotCount, float slotLengthM);
  // Strecke und Sprit dazu; läuft der Abschnitt voll, geht es anteilig im nächsten weiter
  void add(float dm, float dml);
  float sumM() const;
  float sumMl() const;
  // l/100 km über das Fenster; NAN, solange weniger als AVG_MIN_FRACTION der Fensterlänge gefahren ist
  float l100() const;
};

// Schnitt aus Summen (Fahrt, Tank, seit Profilanlage); NAN unter AVG_SIMPLE_MIN_KM
float simpleL100(float km, float liters);

// Tank-Schnitt (A7): seit der letzten Tankfüllung; in den ersten 30 km gilt der Schnitt der
// vorigen Füllung (prevFillL100), falls es einen gibt.
float tankL100(float kmSinceFill, float litersSinceFill, float prevFillL100);

// Die letzten Tankfüllungen (km und berechnete Liter je Füllung) für die Reichweiten-Prognose
struct FillHistory {
  static constexpr uint8_t SIZE = 5;
  float km[SIZE];
  float liters[SIZE];
  uint8_t count;
  uint8_t next;

  void clear();
  void push(float kmValue, float litersValue);
  // ANNAHME: Ø der letzten Füllungen = Summe Liter ÷ Summe km (nicht Mittel der Einzelwerte),
  // damit kurze Füllungen nicht zu viel Gewicht bekommen. NAN ohne Füllung.
  float l100() const;
};

}  // namespace avg
