# PC-Prüfung (nur für die Entwicklung, nicht Teil der Firmware)

Kompiliert die Oberfläche mit LVGL 9.2.2 auf dem PC (Stubs für Arduino, TFT_eSPI, Wire, FreeRTOS),
spielt mit `harness.cpp` eine simulierte Fahrt samt Gesten ab und speichert Bildschirmfotos als PPM.
LVGL-Quellen: crates.io-Paket `lightvgl-sys` 9.2.2 (Ordner `vendor/lvgl`). Pfade in `build.sh` anpassen.
Aufruf: `EXTRA_DEFS=-DSIMULATE_OBD=1 ./build.sh && SHOTDIR=. ./harness script.txt`
