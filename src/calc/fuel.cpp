#include "fuel.h"

#include "config.h"

namespace fuel {

Source chooseSource(FuelType fuel, bool hasFuelRate, bool hasMaf, bool hasMap, bool hasRpm, bool hasAbsLoad,
                    bool hasLambda) {
  if (hasFuelRate) return Source::FuelRate;
  if (fuel == FuelType::Diesel) return (hasMaf && hasLambda) ? Source::Maf : Source::None;
  if (hasMaf) return Source::Maf;
  if (hasAbsLoad && hasRpm) return Source::AbsLoad;
  if (hasMap && hasRpm) return Source::SpeedDensity;
  return Source::None;
}

float densityGPerL(FuelType fuel) {
  return fuel == FuelType::Diesel ? cfg::DENSITY_DIESEL_G_PER_L : cfg::DENSITY_PETROL_G_PER_L;
}

float speedDensityAirGs(float mapKpa, float displacementL, float rpm, float ve, float iatC) {
  // Luft g/s = MAP[kPa] · Hubraum[l] · Drehzahl/120 · VE / (R_Luft · IAT[K])
  const float kelvin = iatC + cfg::KELVIN_OFFSET;
  if (!(kelvin > 0)) return NAN;
  return mapKpa * displacementL * (rpm / 120.0f) * ve / (cfg::R_AIR_KJ_PER_KG_K * kelvin);
}

float absLoadAirGs(float absLoadPct, float displacementL, float rpm) {
  return absLoadPct / 100.0f * cfg::AIR_STP_G_PER_L * displacementL * (rpm / 120.0f);
}

namespace {

// Kraftstoff g/s aus Luft g/s mit λ und Gemischkorrektur (A7)
float fuelFromAir(float airGs, const Input& in, FuelType fuel) {
  // Diesel nur mit gemeldetem λ (sonst wäre der Wert um das 1,3- bis 3-Fache zu hoch)
  if (fuel == FuelType::Diesel && std::isnan(in.lambda)) return NAN;
  const float stoich = fuel == FuelType::Diesel ? cfg::AFR_STOICH_DIESEL : cfg::AFR_STOICH;
  const float afr = std::isnan(in.lambda) ? stoich : stoich * in.lambda;
  if (!(afr > 0)) return NAN;
  const float trims = (std::isnan(in.stftPct) ? 0.0f : in.stftPct) + (std::isnan(in.ltftPct) ? 0.0f : in.ltftPct);
  return airGs / afr * (1.0f + trims / 100.0f);
}

}  // namespace

float rateLph(Source src, const Engine& e, const Input& in) {
  float lph = NAN;
  switch (src) {
    case Source::FuelRate:
      lph = in.fuelRateLph;
      break;
    case Source::Maf:
      if (!std::isnan(in.mafGs)) lph = fuelFromAir(in.mafGs, in, e.fuel) * 3600.0f / densityGPerL(e.fuel);
      break;
    case Source::AbsLoad:
      if (std::isnan(in.absLoadPct) || std::isnan(in.rpm) || !(e.displacementL > 0)) break;
      lph = fuelFromAir(absLoadAirGs(in.absLoadPct, e.displacementL, in.rpm), in, e.fuel) * 3600.0f / densityGPerL(e.fuel);
      break;
    case Source::SpeedDensity: {
      if (std::isnan(in.mapKpa) || std::isnan(in.rpm)) break;
      const float iat = std::isnan(in.iatC) ? cfg::IAT_FALLBACK_C : in.iatC;
      const float air = speedDensityAirGs(in.mapKpa, e.displacementL, in.rpm, e.ve, iat);
      lph = fuelFromAir(air, in, e.fuel) * 3600.0f / densityGPerL(e.fuel);
      break;
    }
    case Source::None:
      break;
  }
  if (std::isnan(lph)) return NAN;
  // ANNAHME: fuel_cal gilt für alle Quellen, auch für 0x5E; die Kalibrierung gleicht jede aus.
  lph *= e.fuelCal;
  return lph < 0 ? 0.0f : lph;
}

bool isFuelCut(bool hasFuelSys, const Input& in, float throttleClosedPct) {
  if (hasFuelSys && !std::isnan(in.fuelSys) && static_cast<int>(in.fuelSys) == cfg::FUEL_SYS_DECEL_CUT) return true;
  if (std::isnan(in.rpm) || std::isnan(in.speedKmh)) return false;
  if (in.rpm <= cfg::CUT_FALLBACK_MIN_RPM || in.speedKmh <= cfg::CUT_FALLBACK_MIN_SPEED_KMH) return false;
  // Gaspedal losgelassen ist das sicherste Zeichen; ohne Pedal die Drosselklappe
  if (!std::isnan(in.pedalPct)) {
    const float closed = std::isnan(in.pedalClosedPct) ? 0.0f : in.pedalClosedPct;
    return in.pedalPct <= closed + cfg::CUT_PEDAL_MARGIN_PCT;
  }
  if (std::isnan(in.throttlePct)) return false;
  const float closed = std::isnan(throttleClosedPct) ? 0.0f : throttleClosedPct;
  return in.throttlePct <= closed + cfg::CUT_THROTTLE_MARGIN_PCT;
}

float litersPer100(float lph, float speedKmh) {
  if (std::isnan(lph) || std::isnan(speedKmh) || speedKmh < cfg::L100_MIN_SPEED_KMH) return NAN;
  return lph / speedKmh * 100.0f;
}

float calibrate(float fuelCal, float filledL, float computedL, float km, bool& applied) {
  applied = false;
  if (!(computedL > 0) || !(filledL > 0) || km < cfg::CAL_MIN_KM) return fuelCal;
  const float ratio = filledL / computedL;
  if (ratio < cfg::CAL_RATIO_MIN || ratio > cfg::CAL_RATIO_MAX) return fuelCal;
  float cal = fuelCal * std::sqrt(ratio);  // halbe Korrektur je Füllung, dämpft Ausreißer
  if (cal < cfg::FUEL_CAL_MIN) cal = cfg::FUEL_CAL_MIN;
  if (cal > cfg::FUEL_CAL_MAX) cal = cfg::FUEL_CAL_MAX;
  applied = true;
  return cal;
}

float tankAfterRefuel(float restL, float addedL, bool full, float tankL) {
  if (full) return tankL;
  if (std::isnan(restL)) return NAN;
  const float l = restL + addedL;
  return l > tankL ? tankL : l;
}

float mixPrice(float restL, float oldMix, float addedL, float price) {
  if (std::isnan(oldMix) || std::isnan(restL) || restL <= 0) return price;
  const float total = restL + addedL;
  if (!(total > 0)) return price;
  return (restL * oldMix + addedL * price) / total;
}

float priceFromCents(int cents) { return cents / 100.0f + cfg::PRICE_TENTH_CENTS; }

int centsFromPrice(float price) {
  if (std::isnan(price)) return 0;
  // 1,699 -> 169: die ⁹ abziehen und auf ganze Cent runden
  return static_cast<int>(std::lround((price - cfg::PRICE_TENTH_CENTS) * 100.0f));
}

float prognosis(float avg100, float avg10, float avgFills) {
  float sum = 0, weight = 0;
  if (!std::isnan(avg100)) { sum += cfg::RANGE_W_100 * avg100; weight += cfg::RANGE_W_100; }
  if (!std::isnan(avg10)) { sum += cfg::RANGE_W_10 * avg10; weight += cfg::RANGE_W_10; }
  if (!std::isnan(avgFills)) { sum += cfg::RANGE_W_FILLS * avgFills; weight += cfg::RANGE_W_FILLS; }
  return weight > 0 ? sum / weight : NAN;
}

float rangeKm(float restL, float l100) {
  if (std::isnan(restL) || std::isnan(l100) || l100 <= 0) return NAN;
  return restL / l100 * 100.0f;
}

}  // namespace fuel
