// Antworten eines ELM327-Adapters zerlegen (A7, M Etappe 2). Reines C++ ohne Arduino, nativ testbar.
//
// Der Adapter läuft mit ATE0 (kein Echo), ATL0 (kein Zeilenvorschub), ATS0 (keine Leerzeichen)
// und ATH0 (keine Kopfbytes). Eine Antwort ist dann z. B.
//   KWP, ein Steuergerät:  "410C1AF8"
//   CAN, mehrere PIDs:     "00A\r0:410C1AF80D32\r1:0B1E11220000"   (ISO-TP: Länge, dann Zeilen "n:")
//   Fehler:                "NO DATA", "?", "UNABLE TO CONNECT", "BUS INIT: ...ERROR", "CAN ERROR"
// Leerzeichen werden trotzdem verstanden (falls der Adapter ATS0 nicht kennt).
#pragma once
#include <cstddef>
#include <cstdint>

namespace elmp {

// Art der Antwort
enum class Reply : uint8_t {
  Data,     // Hex-Daten
  Ok,       // "OK" bzw. Text ohne Daten (z. B. ATZ "ELM327 v1.5")
  NoData,   // "NO DATA": Steuergerät kennt die Abfrage nicht oder schweigt
  Unable,   // "UNABLE TO CONNECT", "BUS INIT: ...ERROR": kein Auto am Bus (Zündung aus)
  Error,    // "?", "ERROR", "CAN ERROR", "BUS ERROR", "BUFFER FULL", "STOPPED", "FB ERROR" ...
  Empty,    // nichts außer dem Prompt
};

Reply classify(const char* text);

// Eine Nachricht eines Steuergeräts (bei ISO-TP schon zusammengesetzt)
struct Message {
  uint8_t data[64];
  uint8_t len = 0;
};

// Zerlegt die Antwort in Nachrichten. Zeilen wie "SEARCHING..." oder "BUS INIT: ...OK" werden
// übersprungen. Liefert die Anzahl (höchstens max).
int parseMessages(const char* text, Message* out, int max);

// Ein dekodierter Mode-01-Wert. Für PID 0x01 ist value die MIL (0/1) und value2 die Zahl der Fehlercodes.
struct PidValue {
  uint8_t pid = 0;
  float value = 0;
  float value2 = 0;
};

// Datenbytes eines Mode-01-PIDs, -1 = unbekannt (dann endet die Auswertung dieser Nachricht)
int pidDataLen(uint8_t pid);

// Rechnet die Datenbytes eines PIDs in den Wert um (SAE J1979). false = PID unbekannt.
bool decodePid(uint8_t pid, const uint8_t* d, PidValue& out);

// Alle PIDs aus Mode-01-Antworten (auch mehrere PIDs je Nachricht). Antworten mehrere
// Steuergeräte auf denselben PID, gilt die erste. Liefert die Anzahl.
int decodeMode01(const Message* msgs, int n, PidValue* out, int max);

// Antwort auf 0100, 0120, 0140 ...: 32 Bit für die PIDs base+1 bis base+32 (alle Steuergeräte
// zusammengefasst). false = keine passende Antwort.
bool decodeSupported(const Message* msgs, int n, uint8_t base, uint32_t& mask);

// Trägt die Bits aus decodeSupported in ein Bitfeld für PID 0x00–0xFF ein (Bit pid%8 in Byte pid/8)
void markSupported(uint8_t bits[32], uint8_t base, uint32_t mask);

// Zahl der unterstützten Mess-PIDs (ohne die Verweise 0x20, 0x40 … auf die nächste Liste)
int countSupported(const uint8_t bits[32]);

// VIN aus der Antwort auf 0902 (CAN: eine ISO-TP-Nachricht, KWP: fünf Nachrichten).
// vin bekommt 17 Zeichen plus Nullbyte. false = keine gültige VIN.
bool decodeVin(const Message* msgs, int n, char* vin, size_t size);

// Bordspannung aus ATRV ("12.6V") in Volt
bool parseVoltage(const char* text, float& volts);

// Protokollnummer aus ATDPN ("A6" bzw. "6"), -1 = unbekannt
int parseProtocolNumber(const char* text);

// Kurzer Name des Protokolls (Diagnose, Startbildschirm), "" bei unbekannter Nummer
const char* protocolName(int number);

// CAN-Protokolle (6–9) erlauben mehrere PIDs je Anfrage
bool protocolIsCan(int number);

}  // namespace elmp
