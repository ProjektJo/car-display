#ifndef SIMULATE_OBD
#include "sensor_task.h"

#include <Arduino.h>
#include <TinyGPS++.h>
#include <Wire.h>

#include <cmath>

#include "config.h"
#include "core/car_state_store.h"
#include "core/commands.h"
#include "hw/i2c_bus.h"
#include "sensors/imu_math.h"
#include "sensors/sun_time.h"
#include "storage/storage_task.h"
#include "storage/store.h"

namespace sensors {

namespace {

// MPU6050-Register
constexpr uint8_t REG_SMPLRT_DIV = 0x19, REG_CONFIG = 0x1A, REG_GYRO_CONFIG = 0x1B, REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t REG_ACCEL_OUT = 0x3B, REG_PWR_MGMT_1 = 0x6B, REG_WHO_AM_I = 0x75;
constexpr float ACCEL_LSB_PER_G = 8192.0f;  // ±4 g
constexpr float GYRO_LSB_PER_DPS = 131.0f;  // ±250 °/s
constexpr uint32_t PUBLISH_MS = 100;        // 10-mal pro Sekunde in den CarState
constexpr uint32_t GPS_TIMEOUT_MS = 5000;   // ohne neue NMEA-Sätze gilt GPS als weg

volatile bool relearn = false;
TinyGPSPlus gps;

bool writeReg(uint8_t reg, uint8_t v) {
  i2cbus::Guard bus;
  Wire.beginTransmission(BOARD_IMU_I2C_ADDR);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

bool readRegs(uint8_t reg, uint8_t* out, uint8_t n) {
  i2cbus::Guard bus;
  Wire.beginTransmission(BOARD_IMU_I2C_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(static_cast<uint8_t>(BOARD_IMU_I2C_ADDR), n) != n) return false;
  for (uint8_t i = 0; i < n; i++) out[i] = Wire.read();
  return true;
}

// MPU6050 beim Start über den I2C-Scan erkennen (A3) und einstellen
bool imuInit() {
  uint8_t who = 0;
  if (!readRegs(REG_WHO_AM_I, &who, 1)) return false;
  // ANNAHME: auch Nachbauten mit anderer Kennung (0x70, 0x72, 0x98) werden akzeptiert
  if (who != 0x68 && who != 0x70 && who != 0x72 && who != 0x98) return false;
  return writeReg(REG_PWR_MGMT_1, 0x01) &&     // aufwecken, Takt vom Gyro
         writeReg(REG_CONFIG, 0x03) &&         // Tiefpass 44 Hz
         writeReg(REG_SMPLRT_DIV, 9) &&        // 100 Hz
         writeReg(REG_GYRO_CONFIG, 0x00) &&    // ±250 °/s
         writeReg(REG_ACCEL_CONFIG, 0x08);     // ±4 g
}

bool imuRead(imu::Vec& a, imu::Vec& g, float& tempC) {
  uint8_t b[14];
  if (!readRegs(REG_ACCEL_OUT, b, sizeof(b))) return false;
  auto s16 = [&](int i) { return static_cast<int16_t>((b[i] << 8) | b[i + 1]); };
  const float k = cfg::GRAVITY / ACCEL_LSB_PER_G;
  a = {s16(0) * k, s16(2) * k, s16(4) * k};
  tempC = s16(6) / 340.0f + 36.53f;
  const float r = static_cast<float>(DEG_TO_RAD) / GYRO_LSB_PER_DPS;  // DEG_TO_RAD aus Arduino.h
  g = {s16(8) * r, s16(10) * r, s16(12) * r};
  return true;
}

void axesToArray(const imu::Axes& ax, float out[6]) {
  out[0] = ax.up.x;
  out[1] = ax.up.y;
  out[2] = ax.up.z;
  out[3] = ax.fwd.x;
  out[4] = ax.fwd.y;
  out[5] = ax.fwd.z;
}

}  // namespace

void requestRelearn() { relearn = true; }

void task(void*) {
  const bool hasImu = imuInit();
  Serial.printf("Sensoren: MPU6050 %s\n", hasImu ? "erkannt" : "nicht vorhanden");
  Serial1.begin(cfg::GPS_BAUD, SERIAL_8N1, BOARD_PIN_GPS_RX, BOARD_PIN_GPS_TX);
  carstate::modify([&](CarState& s) { s.hasImu = hasImu; });

  imu::Learner learner;
  imu::Fusion fusion;
  uint8_t axesProfile = 0;     // Profil, dessen Einbaulage geladen ist
  bool axesSaved = false;
  uint32_t lastPublish = 0, lastNmea = 0, lastKmf = 0;
  uint32_t lastSentences = 0;
  bool gpsSeen = false;
  double kmfGpsKm = 0, kmfObdKm = 0;
  float tempC = NAN;
  TickType_t wake = xTaskGetTickCount();
  const float dtS = cfg::SENSOR_PERIOD_MS / 1000.0f;

  for (;;) {
    const uint32_t now = millis();
    // Werte aus dem CarState: Profil, OBD-Tempo und dessen Beschleunigung
    uint8_t profileId = 0;
    float speed = NAN, obdAccel = NAN;
    {
      CarState& s = carstate::lock();
      profileId = s.profile.id;
      speed = s.speed.get(now);
      obdAccel = s.accel.get(now);
      carstate::unlock();
    }

    // --- MPU6050: Einbaulage je Profil (A10) ---
    if (hasImu) {
      if (profileId != axesProfile || relearn) {
        axesProfile = profileId;
        learner.reset();
        axesSaved = false;
        float arr[6];
        if (relearn) {
          relearn = false;
          if (profileId) {
            const float zero[6] = {0, 0, 0, 0, 0, 0};
            storage::saveImuAxes(profileId, zero);  // vergessen
          }
          Serial.println("Sensoren: Einbaulage wird neu gelernt");
        } else if (profileId && store::loadImuAxes(profileId, arr)) {
          imu::Axes ax;
          ax.up = {arr[0], arr[1], arr[2]};
          ax.fwd = {arr[3], arr[4], arr[5]};
          ax.valid = true;
          fusion.setAxes(ax);
          axesSaved = true;
        }
        if (!axesSaved) fusion.setAxes(imu::Axes{});
      }
      imu::Vec a, g;
      if (imuRead(a, g, tempC)) {
        if (!axesSaved) {
          learner.add(a, g, speed, obdAccel, dtS);
          if (learner.done() && profileId) {
            fusion.setAxes(learner.axes());
            float arr[6];
            axesToArray(learner.axes(), arr);
            storage::saveImuAxes(profileId, arr);
            axesSaved = true;
            Serial.println("Sensoren: Einbaulage gelernt und gespeichert");
          }
        } else {
          fusion.add(a, g, speed, obdAccel, dtS);
        }
      }
    }

    // --- GPS (TinyGPSPlus) ---
    while (Serial1.available() > 0) gps.encode(static_cast<char>(Serial1.read()));
    if (gps.passedChecksum() != lastSentences) {
      lastSentences = gps.passedChecksum();
      lastNmea = now;
      if (!gpsSeen) {
        gpsSeen = true;
        Serial.println("Sensoren: GPS erkannt");
      }
    }
    const bool hasGps = gpsSeen && now - lastNmea < GPS_TIMEOUT_MS;
    const bool fix = hasGps && gps.location.isValid() && gps.location.age() < 2000;

    // km-Faktor (A7): GPS-Strecke gegen OBD-Strecke bei gutem Empfang
    if (fix && now - lastKmf >= 1000) {
      const float dt = lastKmf ? (now - lastKmf) / 1000.0f : 0.0f;
      lastKmf = now;
      const float gpsKmh = gps.speed.isValid() ? static_cast<float>(gps.speed.kmph()) : NAN;
      if (dt > 0 && dt < 3 && gps.satellites.value() >= cfg::KMF_MIN_SATS && !std::isnan(speed) && !std::isnan(gpsKmh) &&
          speed > cfg::KMF_MIN_SPEED_KMH) {
        kmfGpsKm += gpsKmh * dt / 3600.0;
        kmfObdKm += speed * dt / 3600.0;
        if (kmfGpsKm >= cfg::KMF_MIN_KM && kmfObdKm > 0) {
          Command c{CmdType::SetKmFactor};
          c.f = static_cast<float>(kmfGpsKm / kmfObdKm);
          commands::toCalc(c);
          Serial.printf("Sensoren: km-Faktor laut GPS %.3f\n", c.f);
          kmfGpsKm = kmfObdKm = 0;
        }
      }
    }

    // --- 10-mal pro Sekunde in den CarState ---
    if (now - lastPublish >= PUBLISH_MS) {
      lastPublish = now;
      suntime::DateTime utc{};
      const bool timeOk = hasGps && gps.date.isValid() && gps.time.isValid() && gps.date.year() >= 2024 && gps.time.age() < 3000;
      if (timeOk) utc = {gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second()};
      const suntime::DateTime loc = timeOk ? suntime::toLocal(utc) : suntime::DateTime{};
      const float night = (timeOk && fix) ? suntime::nightFactor(utc, static_cast<float>(gps.location.lat()),
                                                                 static_cast<float>(gps.location.lng()))
                                          : NAN;
      const bool ready = hasImu && axesSaved;
      carstate::modify([&](CarState& s) {
        s.imuReady = ready;
        if (ready) {
          s.imuLong.set(fusion.longMs2(), now);
          s.imuLat.set(fusion.latMs2(), now);
          s.slopePct.set(fusion.slopePct(), now);
        }
        s.hasGps = hasGps;
        s.gpsFix = fix;
        s.gpsSats = static_cast<uint8_t>(gps.satellites.isValid() ? gps.satellites.value() : 0);
        // GPS-Tempo für die Anzeige, nur bei gutem Empfang
        if (fix && gps.speed.isValid() && gps.speed.age() < 2000 && s.gpsSats >= cfg::KMF_MIN_SATS)
          s.gpsSpeed.set(static_cast<float>(gps.speed.kmph()), now);
        s.gpsTimeValid = timeOk;
        if (timeOk) {
          s.gpsHour = static_cast<uint8_t>(loc.hour);
          s.gpsMinute = static_cast<uint8_t>(loc.minute);
          s.gpsDate = static_cast<uint32_t>(loc.year * 10000 + loc.month * 100 + loc.day);
          s.gpsEpoch = static_cast<uint32_t>(suntime::toEpoch(utc));
        }
        s.nightFactor = night;
      });
    }
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(cfg::SENSOR_PERIOD_MS));
  }
}

}  // namespace sensors
#else
// Simulator: keine echten Sensoren, die Einbaulage gilt als gelernt
#include "sensor_task.h"
namespace sensors {
void requestRelearn() {}
}  // namespace sensors
#endif
