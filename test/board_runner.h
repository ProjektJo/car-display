// Unit-Tests auf dem Board statt auf dem PC (Umgebung board_test, wenn kein g++ installiert ist).
// Arduino ruft setup() statt main() auf; setup() startet deshalb die Tests. Auf dem PC ohne Wirkung.
#pragma once
#ifdef ARDUINO
#include <Arduino.h>
// Die Tests legen große Strukturen (Fahrzeugrechnung, CarState) auf den Stack; 8 kB reichen dafür nicht
SET_LOOP_TASK_STACK_SIZE(32 * 1024);
void setup() {
  delay(2000);  // USB-Monitor des Testlaufs verbinden lassen
  main();
}
void loop() {}
#endif
