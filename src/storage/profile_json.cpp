#include "profile_json.h"

#include <ArduinoJson.h>

#include <cstdio>
#include <cstring>

namespace {

// PID-Liste wie in den Antworten 0100, 0120 … : je 8 Hex-Zeichen, Bit 31 = erster PID der Gruppe
void maskHex(const uint8_t bits[32], char* out) {
  for (int g = 0; g < 8; g++) {
    uint32_t mask = 0;
    for (int i = 0; i < 32; i++) {
      const int pid = g * 0x20 + 1 + i;
      if (pid <= 0xFF && ((bits[pid / 8] >> (pid % 8)) & 1)) mask |= 1u << (31 - i);
    }
    snprintf(out + g * 8, 9, "%08X", static_cast<unsigned>(mask));
  }
}

// Grenzen; NaN wird zur Untergrenze
float clampf(float v, float lo, float hi) { return v >= lo ? (v <= hi ? v : hi) : lo; }

bool parseMaskHex(const char* hex, uint8_t bits[32]) {
  memset(bits, 0, 32);
  if (!hex || strlen(hex) != 64) return false;
  for (int g = 0; g < 8; g++) {
    char part[9];
    memcpy(part, hex + g * 8, 8);
    part[8] = '\0';
    char* end = nullptr;
    const uint32_t mask = static_cast<uint32_t>(strtoul(part, &end, 16));
    if (end != part + 8) return false;
    for (int i = 0; i < 32; i++) {
      const int pid = g * 0x20 + 1 + i;
      if (pid <= 0xFF && (mask >> (31 - i)) & 1u) bits[pid / 8] |= static_cast<uint8_t>(1u << (pid % 8));
    }
  }
  return true;
}

}  // namespace

size_t profileToJson(const Profile& p, char* out, size_t size) {
  JsonDocument doc;
  doc["name"] = p.name;
  doc["vin"] = p.vin;
  doc["fuel"] = p.fuel == FuelType::Diesel ? "diesel" : "petrol";
  doc["displacement_l"] = p.displacementL;
  doc["tank_l"] = p.tankL;
  if (p.reserveL >= 0) doc["reserve_l"] = p.reserveL;
  doc["ve"] = p.ve;
  doc["fuel_cal"] = p.fuelCal;
  JsonArray gears = doc["gears"].to<JsonArray>();
  for (uint8_t i = 0; i < p.gearCount && i < MAX_GEARS; i++) gears.add(p.gears[i]);
  char hex[65];
  maskHex(p.supported, hex);
  doc["supported_pids"] = hex;
  doc["protocol"] = "auto";            // Adapter sucht selbst (ATSP0, A6)
  doc["protocol_id"] = p.protocol;     // gefundenes Protokoll, für die Erkennung
  doc["cold_rpm_limit"] = p.coldRpmLimit;
  doc["cold_coolant_c"] = p.coldCoolantC;
  doc["body"] = p.bodyType().key;
  doc["power_kw"] = p.powerKw;
  doc["redline_rpm"] = p.redlineRpm;
  doc["shift_rpm"] = p.shiftRpm;
  doc["km_factor"] = p.kmFactor;
  return serializeJsonPretty(doc, out, size);
}

bool profileFromJson(const char* json, Profile& p) {
  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return false;
  const char* name = doc["name"] | "";
  if (!name[0]) return false;
  Profile d;  // Standardwerte für fehlende Felder
  d.id = p.id;
  snprintf(d.name, sizeof(d.name), "%s", name);
  snprintf(d.vin, sizeof(d.vin), "%s", doc["vin"] | "");
  d.fuel = strcmp(doc["fuel"] | "petrol", "diesel") == 0 ? FuelType::Diesel : FuelType::Petrol;
  d.displacementL = doc["displacement_l"] | d.displacementL;
  d.tankL = doc["tank_l"] | d.tankL;
  d.reserveL = doc["reserve_l"] | d.reserveL;
  d.ve = doc["ve"] | d.ve;
  d.fuelCal = doc["fuel_cal"] | d.fuelCal;
  JsonArrayConst gears = doc["gears"].as<JsonArrayConst>();
  d.gearCount = 0;
  for (JsonVariantConst g : gears) {
    if (d.gearCount >= MAX_GEARS) break;
    d.gears[d.gearCount++] = g.as<float>();
  }
  parseMaskHex(doc["supported_pids"] | "", d.supported);
  d.protocol = doc["protocol_id"] | static_cast<int8_t>(-1);
  d.coldRpmLimit = doc["cold_rpm_limit"] | d.coldRpmLimit;
  d.coldCoolantC = doc["cold_coolant_c"] | d.coldCoolantC;
  const char* body = doc["body"] | "";
  for (uint8_t i = 0; i < cfg::BODY_TYPE_COUNT; i++)
    if (strcmp(body, cfg::BODY_TYPES[i].key) == 0) d.body = i;
  d.powerKw = doc["power_kw"] | d.powerKw;
  d.redlineRpm = doc["redline_rpm"] | d.redlineRpm;
  d.shiftRpm = doc["shift_rpm"] | d.shiftRpm;
  d.kmFactor = doc["km_factor"] | d.kmFactor;
  // Unsinnige Werte (z. B. von Hand bearbeitete Datei) auf die Grenzen des Assistenten bzw. der
  // Kalibrierung setzen, damit die Rechnung nie durch 0 teilt
  d.displacementL = clampf(d.displacementL, cfg::DISPLACEMENT_MIN_L, cfg::DISPLACEMENT_MAX_L);
  d.tankL = clampf(d.tankL, cfg::TANK_MIN_L, cfg::TANK_MAX_L);
  d.fuelCal = clampf(d.fuelCal, cfg::FUEL_CAL_MIN, cfg::FUEL_CAL_MAX);
  d.ve = clampf(d.ve, 0.5f, 1.1f);  // ANNAHME: üblicher Bereich des Füllungsgrads
  if (!(d.kmFactor > 0.5f && d.kmFactor < 1.5f)) d.kmFactor = cfg::DEFAULT_KM_FACTOR;
  p = d;
  return true;
}
