// Fehlercodes (A11): Mode 03 (gespeichert) und 07 (vorläufig) zerlegen, als Text "P0171" schreiben und
// den deutschen Klartext aus der eingebauten Tabelle (data/dtc_de.csv -> dtc_table.cpp) suchen.
// Reines C++ ohne Arduino, nativ testbar.
#pragma once
#include <cstddef>
#include <cstdint>

#include "obd/elm_parser.h"

namespace dtc {

// Fehlercode als 16 Bit wie im OBD-Protokoll: Bits 15–14 Buchstabe (P, C, B, U), dann vier Ziffern
using Code = uint16_t;

// Codes aus den Antworten auf Mode 03 bzw. 07 (mode = 0x03 / 0x07). CAN-Antworten tragen nach dem
// Kennbyte die Anzahl, KWP-Antworten bis zu drei Codes je Nachricht, mit 0000 aufgefüllt.
// Doppelte Codes (mehrere Steuergeräte) zählen einmal. Liefert die Anzahl (höchstens max).
int decode(const elmp::Message* msgs, int n, uint8_t mode, Code* out, int max);

// "P0171"; out braucht mindestens 6 Zeichen
void format(Code c, char* out, size_t size);

// Deutscher Klartext, "Herstellerspezifischer Code" für unbekannte bzw. herstellerspezifische Codes
const char* text(Code c);

// Für Tests: Zahl der Einträge in der Tabelle
int tableSize();

}  // namespace dtc
