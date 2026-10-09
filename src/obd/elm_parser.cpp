#include "elm_parser.h"

#include <cmath>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace elmp {

namespace {

constexpr size_t LINE_MAX = 160;

int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

bool allHex(const char* s, size_t n) {
  if (n == 0) return false;
  for (size_t i = 0; i < n; i++)
    if (hexVal(s[i]) < 0) return false;
  return true;
}

// Nächste Zeile aus text ab pos, ohne Leerzeichen, Tabs und Prompt. false = Text zu Ende.
bool nextLine(const char* text, size_t& pos, char* line, size_t& len) {
  if (!text[pos]) return false;
  len = 0;
  while (text[pos] && text[pos] != '\r' && text[pos] != '\n') {
    const char c = text[pos++];
    if (c == ' ' || c == '\t' || c == '>') continue;
    if (len < LINE_MAX - 1) line[len++] = c;
  }
  while (text[pos] == '\r' || text[pos] == '\n') pos++;
  line[len] = '\0';
  return true;
}

// Hängt Hex-Paare an eine Nachricht an
void appendHex(Message& m, const char* hex, size_t n) {
  for (size_t i = 0; i + 1 < n && m.len < sizeof(m.data); i += 2)
    m.data[m.len++] = static_cast<uint8_t>(hexVal(hex[i]) * 16 + hexVal(hex[i + 1]));
}

bool contains(const char* text, const char* what) { return strstr(text, what) != nullptr; }

}  // namespace

Reply classify(const char* text) {
  // Großbuchstaben-Kopie für die Suche nach Fehlertexten
  char up[256];
  size_t n = 0;
  for (; text[n] && n < sizeof(up) - 1; n++) up[n] = static_cast<char>(toupper(static_cast<unsigned char>(text[n])));
  up[n] = '\0';

  if (contains(up, "UNABLE TO CONNECT") || (contains(up, "BUS INIT") && contains(up, "ERROR"))) return Reply::Unable;
  if (contains(up, "NO DATA")) return Reply::NoData;
  if (contains(up, "ERROR") || contains(up, "STOPPED") || contains(up, "BUFFER FULL") || contains(up, "?"))
    return Reply::Error;

  bool anyText = false;
  size_t pos = 0, len = 0;
  char line[LINE_MAX];
  while (nextLine(text, pos, line, len)) {
    if (len == 0) continue;
    anyText = true;
    const char* colon = strchr(line, ':');
    if (allHex(line, len)) return Reply::Data;
    if (colon && colon - line == 1 && hexVal(line[0]) >= 0 && allHex(colon + 1, strlen(colon + 1))) return Reply::Data;
  }
  return anyText ? Reply::Ok : Reply::Empty;
}

int parseMessages(const char* text, Message* out, int max) {
  int count = 0;
  int isoIndex = -1;      // Nachricht, an die "n:"-Zeilen angehängt werden
  int isoLen = 0;         // angekündigte Länge der ISO-TP-Nachricht
  size_t pos = 0, len = 0;
  char line[LINE_MAX];

  auto finishIso = [&]() {
    if (isoIndex >= 0 && isoLen > 0 && out[isoIndex].len > isoLen) out[isoIndex].len = static_cast<uint8_t>(isoLen);
    isoIndex = -1;
    isoLen = 0;
  };

  while (nextLine(text, pos, line, len)) {
    if (len == 0) continue;
    const char* colon = strchr(line, ':');
    if (colon && colon - line == 1 && hexVal(line[0]) >= 0) {
      // Folgezeile "n:..." einer ISO-TP-Nachricht; ohne Längenzeile beginnt "0:" eine neue
      const char* hex = colon + 1;
      const size_t hexLen = strlen(hex);
      if (!allHex(hex, hexLen)) continue;
      if (isoIndex < 0 || line[0] == '0') {
        if (isoIndex >= 0 && line[0] == '0' && out[isoIndex].len > 0) finishIso();
        if (isoIndex < 0) {
          if (count >= max) break;
          out[count] = Message{};
          isoIndex = count++;
        }
      }
      appendHex(out[isoIndex], hex, hexLen);
      continue;
    }
    if (!allHex(line, len)) continue;  // SEARCHING..., BUS INIT: ...OK, Text
    if (len == 3) {
      // Längenzeile einer ISO-TP-Nachricht (drei Hex-Ziffern = Zahl der Bytes)
      finishIso();
      if (count >= max) break;
      out[count] = Message{};
      isoIndex = count++;
      isoLen = static_cast<int>(strtol(line, nullptr, 16));
      continue;
    }
    finishIso();
    if (count >= max) break;
    out[count] = Message{};
    appendHex(out[count], line, len);
    count++;
  }
  finishIso();
  return count;
}

int pidDataLen(uint8_t pid) {
  switch (pid) {
    case 0x00: case 0x20: case 0x40: case 0x60: case 0x80: case 0xA0: case 0xC0: case 0xE0:
    case 0x01:
      return 4;
    case 0x03: case 0x0C: case 0x10: case 0x1F: case 0x21: case 0x31: case 0x42: case 0x43: case 0x44: case 0x5E:
    case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1A: case 0x1B:
      return 2;
    case 0x04: case 0x05: case 0x06: case 0x07: case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0D:
    case 0x0E: case 0x0F: case 0x11: case 0x2F: case 0x33: case 0x45: case 0x46: case 0x47: case 0x49:
    case 0x4A: case 0x4C: case 0x51: case 0x5A: case 0x5C:
      return 1;
    default:
      return -1;
  }
}

bool decodePid(uint8_t pid, const uint8_t* d, PidValue& out) {
  out = PidValue{};
  out.pid = pid;
  const float a = d[0];
  const float b = pidDataLen(pid) >= 2 ? d[1] : 0;
  switch (pid) {
    case 0x01:  // MIL in Bit 7, Zahl der gespeicherten Fehlercodes in Bit 0–6
      out.value = (d[0] & 0x80) ? 1.0f : 0.0f;
      out.value2 = static_cast<float>(d[0] & 0x7F);
      return true;
    case 0x03:  // Kraftstoffsystem 1 (Bitfeld: 1 offen kalt, 2 geregelt, 4 offen wegen Schub ...)
      out.value = a;
      return true;
    case 0x04: case 0x11: case 0x2F: case 0x45: case 0x47: case 0x49: case 0x4A: case 0x4C: case 0x5A:
      out.value = a * 100.0f / 255.0f;  // Prozent
      return true;
    case 0x05: case 0x0F: case 0x46: case 0x5C:
      out.value = a - 40.0f;  // °C
      return true;
    case 0x06: case 0x07: case 0x08: case 0x09:
      out.value = (a - 128.0f) * 100.0f / 128.0f;  // Gemischkorrektur %
      return true;
    case 0x0A:
      out.value = a * 3.0f;  // kPa
      return true;
    case 0x0B: case 0x0D: case 0x33:
      out.value = a;  // kPa bzw. km/h
      return true;
    case 0x0C:
      out.value = (a * 256.0f + b) / 4.0f;  // U/min
      return true;
    case 0x0E:
      out.value = a / 2.0f - 64.0f;  // Zündzeitpunkt °
      return true;
    case 0x10:
      out.value = (a * 256.0f + b) / 100.0f;  // MAF g/s
      return true;
    case 0x1F: case 0x21: case 0x31:
      out.value = a * 256.0f + b;  // s bzw. km
      return true;
    case 0x42:
      out.value = (a * 256.0f + b) / 1000.0f;  // Steuergerät-Spannung V
      return true;
    case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1A: case 0x1B:
      out.value = a / 200.0f;  // Lambdasonde Spannung V
      out.value2 = b == 0xFF ? NAN : (b - 128.0f) * 100.0f / 128.0f;  // Kurzzeitkorrektur dieser Sonde %
      return true;
    case 0x43:
      out.value = (a * 256.0f + b) * 100.0f / 255.0f;  // absolute Last % (Luft je Hub, normiert)
      return true;
    case 0x44:
      out.value = (a * 256.0f + b) * 2.0f / 65536.0f;  // Soll-Lambda
      return true;
    case 0x51:
      out.value = a;  // Kraftstoffart
      return true;
    case 0x5E:
      out.value = (a * 256.0f + b) / 20.0f;  // Kraftstoff l/h
      return true;
    default:
      return false;
  }
}

int decodeMode01(const Message* msgs, int n, PidValue* out, int max) {
  int count = 0;
  for (int m = 0; m < n; m++) {
    const Message& msg = msgs[m];
    if (msg.len < 2 || msg.data[0] != 0x41) continue;
    int i = 1;
    while (i < msg.len) {
      const uint8_t pid = msg.data[i];
      const int dl = pidDataLen(pid);
      if (dl < 0 || i + 1 + dl > msg.len) break;
      bool seen = false;
      for (int k = 0; k < count; k++) seen = seen || out[k].pid == pid;
      PidValue v;
      if (!seen && count < max && decodePid(pid, &msg.data[i + 1], v)) out[count++] = v;
      i += 1 + dl;
    }
  }
  return count;
}

bool decodeSupported(const Message* msgs, int n, uint8_t base, uint32_t& mask) {
  bool found = false;
  mask = 0;
  for (int m = 0; m < n; m++) {
    const Message& msg = msgs[m];
    if (msg.len < 6 || msg.data[0] != 0x41 || msg.data[1] != base) continue;
    mask |= (static_cast<uint32_t>(msg.data[2]) << 24) | (static_cast<uint32_t>(msg.data[3]) << 16) |
            (static_cast<uint32_t>(msg.data[4]) << 8) | msg.data[5];
    found = true;
  }
  return found;
}

void markSupported(uint8_t bits[32], uint8_t base, uint32_t mask) {
  for (int k = 0; k < 32; k++) {
    if (!(mask & (1u << (31 - k)))) continue;
    const int pid = base + 1 + k;
    if (pid > 0xFF) break;
    bits[pid / 8] |= static_cast<uint8_t>(1u << (pid % 8));
  }
}

int countSupported(const uint8_t bits[32]) {
  int n = 0;
  for (int pid = 1; pid <= 0xFF; pid++) {
    if (pid % 0x20 == 0) continue;  // Verweis auf die nächste Liste, kein Messwert
    if ((bits[pid / 8] >> (pid % 8)) & 1) n++;
  }
  return n;
}

bool decodeVin(const Message* msgs, int n, char* vin, size_t size) {
  if (size < 18) return false;
  // Nachrichten "49 02 nn ..." nach der Folgenummer nn ordnen (KWP schickt fünf Teile)
  char raw[64];
  size_t rawLen = 0;
  for (int seq = 0; seq <= 5; seq++) {
    for (int m = 0; m < n; m++) {
      const Message& msg = msgs[m];
      if (msg.len < 4 || msg.data[0] != 0x49 || msg.data[1] != 0x02 || msg.data[2] != seq) continue;
      for (int i = 3; i < msg.len && rawLen < sizeof(raw); i++) {
        const char c = static_cast<char>(msg.data[i]);
        if (isalnum(static_cast<unsigned char>(c))) raw[rawLen++] = c;  // Füllbytes 00 fallen weg
      }
      break;  // nur das erste Steuergerät
    }
  }
  if (rawLen < 17) return false;
  memcpy(vin, raw + rawLen - 17, 17);
  vin[17] = '\0';
  return true;
}

bool parseVoltage(const char* text, float& volts) {
  while (*text && !isdigit(static_cast<unsigned char>(*text))) text++;
  if (!*text) return false;
  char buf[16];
  size_t n = 0;
  for (; *text && n < sizeof(buf) - 1; text++) {
    if (isdigit(static_cast<unsigned char>(*text)))
      buf[n++] = *text;
    else if (*text == '.' || *text == ',')
      buf[n++] = '.';
    else
      break;
  }
  buf[n] = '\0';
  volts = static_cast<float>(atof(buf));
  return volts > 0.0f && volts < 40.0f;
}

int parseProtocolNumber(const char* text) {
  size_t pos = 0, len = 0;
  char line[LINE_MAX];
  while (nextLine(text, pos, line, len)) {
    const char* p = line;
    if ((*p == 'A' || *p == 'a') && p[1]) p++;  // "A6" = automatisch gefunden
    if (strlen(p) == 1 && hexVal(*p) >= 0) return hexVal(*p);
  }
  return -1;
}

const char* protocolName(int number) {
  switch (number) {
    case 1: return "SAE J1850 PWM";
    case 2: return "SAE J1850 VPW";
    case 3: return "ISO 9141-2";
    case 4: return "ISO 14230-4 KWP (5 Baud)";
    case 5: return "ISO 14230-4 KWP";
    case 6: return "ISO 15765-4 CAN 11/500";
    case 7: return "ISO 15765-4 CAN 29/500";
    case 8: return "ISO 15765-4 CAN 11/250";
    case 9: return "ISO 15765-4 CAN 29/250";
    default: return "";
  }
}

bool protocolIsCan(int number) { return number >= 6 && number <= 9; }

}  // namespace elmp
