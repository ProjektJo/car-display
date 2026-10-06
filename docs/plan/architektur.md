# Architektur: Car-Display Firmware v2

Stand 3. Oktober 2026 (nach der Gesamtprüfung), Schritt 2 des Meilensteins. Grundlage: Jos Antworten auf den Fragenkatalog (`meilenstein-bauplan.md`).

## 1. Ziele

1. **Spritsparend fahren:** Der Eco-Modus ist die Startseite und die wichtigste Seite. Alles, was beim Spritsparen hilft (Momentanverbrauch, Schubabschaltung, Schaltempfehlung, Verbrauchsvergleich), steht dort.
2. **Für jeden Fahrer brauchbar:** Übersicht, Großanzeige und Diagramme funktionieren ohne Einarbeitung.
3. **Fahrzeugwechsel ohne Neu-Flashen:** Fahrzeugprofile, die Firmware erkennt selbst, welche Werte das Auto liefert.
4. **Robust im Alltag:** Strom weg mit der Zündung, Daten gehen trotzdem nicht verloren, Bluetooth verbindet sich selbst neu.

## 2. Festgelegte Anforderungen (aus Jos Antworten)

| Nr. | Thema | Entscheidung |
|---|---|---|
| 1 | Fahrzeug | Renault Modus. Fahrzeugwechsel über Profile (siehe 6) |
| 2 | Werte | Nur Standard-OBD2 (Mode 01/03/04/07/09), keine Hersteller-PIDs |
| 3 | Adapter | Vgate vLinker MC+ BT 4.0 (BLE). Hinweis: Nur die Version mit "iOS" bzw. "BT4.0/BLE" funktioniert, die Bluetooth-Classic-Version kann der ESP32-S3 nicht |
| 4 | Strom | USB im Auto, aus mit der Zündung → Firmware speichert laufend (siehe 8) |
| 5 | Helligkeit | Menü → Helligkeit: Modus Tag / Nacht / Auto (GPS) und je eine Helligkeit für Tag (10–100 %, 10er Schritte, Standard 80) und Nacht (1–60 %, unter 5 % 1er, sonst 5er Schritte, Standard 25). Ohne Uhr kein Auto ohne GPS |
| 6 | Ausrichtung | Querformat 320x240 |
| 7 | Hauptwerte | Tempo, Drehzahl, Momentanverbrauch, Durchschnittsverbrauch (mehrere Zeitfenster), Gaspedal, Restreichweite. Eigener Eco-/Hypermiling-Modus |
| 8 | Seiten | Eco (Startseite, enthält die Verbrauchskurve) · Sport · Sprint · Übersicht · Großanzeige · Fahrt & Tank · Diagramme · G-Kraft (nur mit MPU6050) · Historie · Fehlercodes · Info |
| 9 | Diagramm-Zeitfenster | umschaltbar 1 / 5 / 30 min |
| 10 | Kacheln | frei belegbar, wird gespeichert |
| 11 | Bedienung | Wischen = Seite, Tippen = Details, langes Drücken = Menü bzw. Kachel belegen |
| 12 | Einstellungsmenü | ja, am Gerät |
| 13/14 | Warnungen | kalter Motor (Drehzahl-Kachel `warn` und Hinweis-Feld auf der Eco-Seite) und einmalig der Thermostat-Hinweis. Kein Vollbild-Banner, keine Töne |
| 15 | Fehlercodes | lesen mit deutschem Klartext; löschen nur mit Sicherheitsabfrage und bei stehendem Motor |
| 16 | Freeze-Frame/Readiness | nein |
| 17 | Speichern | Fahrtenbuch der letzten 50 Fahrten und 100 Tankfüllungen im Flash, Historie-Seite mit Auswertungen und Diagrammen |
| 18 | Kosten | Tankvorgang automatisch erkennen (wenn PID 0x2F da ist), Tank-Fenster über jeder Seite, Preis mit ▲/▼ je Stelle eingeben (Euro und Cent, ⁹ fest dahinter), Kosten je Fahrt und Tankfüllung |
| 19 | WLAN | keins. Updates per USB |
| 20 | Uhrzeit | Das Board hat **keine RTC** (laut Freenove-Schaltplan). Uhrzeit nur, wenn das optionale GPS steckt |
| 21 | Stil | schlicht, modern, dunkel, zeitlos, keine knalligen Farben |
| 22 | Sprache | Deutsch, metrisch |
| 23 | Start | Verbindungsstatus statt Logo |
| 24 | Basis | bestehende Firmware nur als Referenz für Hardware und BLE |
| 25 | Grafik | LVGL 9 |
| 26 | Lieferung | in Etappen, jede kompiliert |
| 27 | Test | Jo kompiliert lokal mit VS Code + PlatformIO |
| 28/29 | Sensoren | MPU6050 und GPS optional, automatisch erkannt, ohne Sensor sind die zugehörigen Seiten ausgeblendet |

## 3. Hardware

**Board:** Freenove ESP32-S3 Display 2,8" mit Touch, **FNK0104B** (Modul ESP32-S3 N16R8: 16 MB Flash, 8 MB OPI-PSRAM). Pins aus Freenoves eigenen Beispiel-Sketches (Repo `Freenove/Freenove_ESP32_S3_Display`, Zweig für `FNK0104AB_2P8_240x320_ILI9341`):

| Funktion | Pins | Nutzung |
|---|---|---|
| Display ILI9341 (SPI) | MOSI 11, MISO 13, SCLK 12, CS 10, DC 46, RST –, Backlight 45 (PWM) | BGR, Inversion an, 40 MHz SPI |
| Touch FT6336U (I2C 0x38) | SDA 16, SCL 15, RST 18, INT 17 | LVGL-Eingabe |
| I2C-Bus (geteilt) | SDA 16, SCL 15 | Touch, Audio-Codec ES8311 (0x18), optional MPU6050 (0x68) |
| microSD (SD_MMC 4-Bit) | CMD 40, CLK 38, D0 39, D1 41, D2 48, D3 47 | optional: Export der Fahrten als CSV |
| Audio ES8311 + Verstärker | I2S MCK 4, BCK 5, DIN 6, DOUT 8, WS 7, Verstärker-Enable 1 | nicht genutzt (Jo will keine Töne). Enable-Pin auf LOW halten |
| RGB-LED WS2812 | 42 | optional als dezente Statusanzeige |
| Akku-Messung | ADC 9 | nicht genutzt |
| BOOT-Taste | 0 | zweite Eingabe (Seite weiter) |
| Frei am Steckplatz | GPIO 2, 3 (UART), 43/44 (USB-Seriell-Konsole) | optional GPS an UART1 auf 2/3 |

**Hinweis "anderes Display":** Die Firmware trennt alle Board-Angaben in eine Datei `board_fnk0104b.h`. Sollte Jos Board doch ein anderes Freenove-Modell sein (3,5" ST77922 oder 4,0" ST7796), ist nur diese Datei und die Auflösung anzupassen. Das Layout ist für 320x240 entworfen; für 480x320 skaliert LVGL Schriften und Abstände über ein Theme.

**Optionale Sensoren:**
- **MPU6050** an I2C (3,3 V vom I2C-Stecker). Wird beim Start über I2C-Scan erkannt.
- **GPS NEO-6M / NEO-M8N** an UART1, RX = GPIO 2, TX = GPIO 3, 9600 Baud, NMEA. Wird erkannt, sobald gültige NMEA-Sätze ankommen.

## 4. Software-Stack

| Teil | Wahl | Grund |
|---|---|---|
| Build | PlatformIO, `espressif32@^6.9.0`, Arduino-Core 2.x | bewährt in der bestehenden Firmware |
| Grafik | **LVGL 9.2** (`lvgl/lvgl@^9.2.0`) | Wischgesten, Menüs, Diagramme, Animationen |
| Display-Treiber | TFT_eSPI 2.5.43 als Flush-Backend (`lv_tft_espi_create` oder eigener Flush mit DMA) | Pins und Init sind mit Freenoves Setup getestet |
| Touch | FT6336U direkt über `Wire`, als LVGL-indev | wie bestehende Firmware, kein Extra-Treiber |
| Bluetooth | NimBLE-Arduino 1.4.x | Speicher sparsam, bestehender Code `ble_serial.cpp` als Vorlage |
| OBD | **Eigener ELM327-Client** statt ELMduino | brauchen Multi-PID-Abfragen, Fahrzeugprotokoll-Erkennung, nicht-blockierende Queue über BLE. ELMduino bleibt Referenz |
| Dateisystem | LittleFS (eigene Partition, ca. 4 MB) | Fahrten, Tankfüllungen, Profile |
| Einstellungen | Preferences (NVS) | klein, atomar |
| JSON | ArduinoJson 7 | Profile und Export |
| GPS | TinyGPSPlus | optional |

## 5. Aufgaben und Kerne (FreeRTOS)

```
Core 0                                  Core 1
┌────────────────────────┐              ┌──────────────────────────┐
│ obdTask  (Prio 5)      │  CarState    │ uiTask (Prio 4)          │
│  BLE ↔ ELM327-Queue    │ ───────────▶ │  LVGL, 30 fps max        │
│  PID-Scheduler         │  Snapshot    │  Touch, Gesten           │
├────────────────────────┤  (Mutex,     ├──────────────────────────┤
│ sensorTask (Prio 3)    │   10 Hz)     │ storageTask (Prio 1)     │
│  GPS, MPU6050          │              │  LittleFS / NVS schreiben│
├────────────────────────┤              └──────────────────────────┘
│ calcTask (Prio 4)      │
│  Verbrauch, Eco-Score, │
│  Mittelwerte, Fahrt    │
└────────────────────────┘
```

- **CarState** ist eine Struktur mit Rohwerten, abgeleiteten Werten und Zeitstempel je Wert. `calcTask` schreibt, `uiTask` holt sich 10-mal pro Sekunde eine Kopie (Mutex, kurz). Keine LVGL-Aufrufe außerhalb von `uiTask`.
- UI-Aktionen (z. B. "Fahrt zurücksetzen", "Fehlercodes löschen") gehen als Befehle über eine FreeRTOS-Queue an `obdTask` bzw. `calcTask`.
- LVGL-Zeichenpuffer: 2 × 320 × 40 Pixel im internen RAM (DMA-fähig), große Objekte und Diagrammdaten im PSRAM.

## 6. Fahrzeugprofile (Antwort auf "was, wenn ich das Fahrzeug wechsle?")

Ein Profil ist eine JSON-Datei in `/profiles/`:

```json
{ "name": "Renault Modus", "vin": "", "fuel": "petrol",
  "displacement_l": 1.2, "tank_l": 49, "ve": 0.85, "fuel_cal": 1.00,
  "gears": [], "supported_pids": "BE1FA813...", "protocol": "auto",
  "cold_rpm_limit": 2500, "cold_coolant_c": 60, "body": "klein",
  "power_kw": 55, "redline_rpm": 6000, "shift_rpm": 2200, "km_factor": 1.00 }
```

**Fahrzeugart** (`body`, im Menü wählbar, Jos Wunsch vom 3. Oktober): liefert Gewicht inkl. Fahrer und cw·A für Leistung, Bremsenergie und Eco-Score. c_r einheitlich 0,012. Hubraum, Tank, Kraftstoff, Nennleistung (Skala des Leistungsbalkens), Rotbereich und Schaltdrehzahl bleiben im Profil.

| Art | Gewicht | cw·A |
|---|---|---|
| Kleinwagen (Standard, Modus) | 1150 kg | 0,70 m² |
| Kompakt | 1400 kg | 0,68 m² |
| Limousine | 1550 kg | 0,62 m² |
| Kombi | 1600 kg | 0,70 m² |
| SUV | 1850 kg | 0,88 m² |
| Van | 1750 kg | 0,85 m² |
| Transporter | 2300 kg | 1,15 m² |

- **Beim Verbinden:** Adapter-Init, Protokoll automatisch (`ATSP0`), dann `0100/0120/0140/...` lesen = Liste der unterstützten PIDs. Wenn möglich VIN lesen (`0902`).
- **Profil erkennen** (Jos Entscheidung vom 5. Oktober): Liefert das Auto eine VIN, gewinnt das Profil mit derselben VIN. Sonst zählt die PID-Liste: Ein Profil passt, wenn seine gespeicherte Liste der unterstützten PIDs und das gefundene Protokoll genau gleich sind (ein Profil mit einer anderen VIN passt nie). Passt genau ein Profil, wird es geladen; passen mehrere oder keins, fragt das Display einmal: "Welches Fahrzeug?" mit Liste und "Neues Fahrzeug" (Assistent: Name, Kraftstoff, Hubraum, Tankgröße). Das gewählte bzw. neue Profil merkt sich VIN, PID-Liste und Protokoll dieses Autos. Die Bluetooth-Adresse des Adapters wird bewusst nicht genutzt.
- **Gänge werden berechnet, nicht ausgelesen.** Über Standard-OBD liefert kaum ein Auto den Gang, deshalb nutzt die Firmware nur Tempo und Drehzahl, die jedes Auto liefert:
  - Kennzahl `k = Tempo ÷ Drehzahl × 1000` (km/h je 1000 U/min). Jeder Gang hat einen festen Wert, beim Modus z. B. grob 7 / 13 / 19 / 26 / 32.
  - **Lernen:** Nur stabile Phasen zählen (Tempo > 10 km/h, Drehzahl > 1100, k ändert sich über 1,5 s um weniger als 3 %, Gaspedal > 0). Diese k-Werte landen in einem Histogramm; die Häufungen sind die Gänge. Nach etwa 20–30 Minuten gemischter Fahrt sind alle Gänge erkannt, gespeichert im Fahrzeugprofil.
  - **Abkürzung:** Im Profil lassen sich die Übersetzungen auch direkt eintragen (aus Datenblatt), dann ist der Gang sofort da.
  - **Anzeige:** Gang = nächster gelernter Wert, wenn k höchstens 6 % davon abweicht. Sonst "–" (Kupplung getreten, Schalten). Drehzahl im Leerlauf bei Fahrt > 15 km/h = "N" (ausgekuppelt rollen, dafür gibt es den Spartipp).
  - Automatik mit Wandler: unscharf, solange der Wandler schlupft; die Anzeige zeigt dann "D". Für den Modus mit Schaltgetriebe kein Thema.
  - Solange noch nichts gelernt ist, bleibt der Gang leer und die Schaltempfehlung nutzt nur die Drehzahl.
  - **Fahrzeug-Prüfung** (Jos Entscheidung vom 5. Oktober): Hat das geladene Profil schon Gänge gelernt und passen die stabilen Phasen dieser Fahrt zu keinem davon, fragt das Display einmal je Einschalten: "Fährst du mit Fahrzeug <Name>?" mit den Knöpfen "Ja" und "Fahrzeug ändern". "Fahrzeug ändern" öffnet "Welches Fahrzeug?". "Ja" behält das Profil, verwirft dessen gelernte Gänge und lernt sie neu (z. B. andere Reifen). Als unpassend gilt eine Fahrt nach mindestens 2 min stabiler Phasen, von denen höchstens ein Drittel einem gelernten Gang entspricht (± 6 %, wie bei der Anzeige).
- Mittelwerte, Tankstand und Fahrten gehören jeweils zu einem Profil.

## 7. OBD-Datenerfassung

### Renault Modus: was zu erwarten ist (gefolgert, am Auto zu prüfen)
- Benziner (1.2 16V D4F, 1.4/1.6 16V) haben einen **Saugrohrdrucksensor (MAP), keinen Luftmassenmesser (MAF)**, und liefern kein PID 0x5E (Fuel Rate). Diesel 1.5 dCi hat MAF.
- Protokoll: vor dem Facelift meist ISO 14230 (KWP2000), später CAN. Auf KWP schafft man nur ca. 4–8 Abfragen pro Sekunde und keine Multi-PID-Anfragen; das Scheduling muss damit auskommen.
- Tankfüllstand (PID 0x2F) und Gaspedal (0x49) liefern ältere Renault oft nicht. Die Firmware hat für beides einen Ersatz (siehe unten).

### PID-Scheduler
| Klasse | PIDs | Ziel-Takt |
|---|---|---|
| schnell | 0x0D Tempo, 0x0C Drehzahl, 0x0B MAP, 0x11 Drosselklappe bzw. 0x49 Gaspedal | jede Runde |
| mittel | 0x0F Ansaugluft, 0x06/0x07 Gemischkorrektur, 0x03 Kraftstoffsystem-Status, 0x04 Last, 0x10 MAF (falls da) | ca. 1 s |
| langsam | 0x05 Kühlmittel, `ATRV` Bordspannung, 0x2F Tank (falls da) | ca. 5 s |
| selten | 0x01 (MIL + Anzahl Fehlercodes) | 30 s |

- Nicht unterstützte PIDs (aus der Bitmaske) werden nie abgefragt.
- Auf CAN: bis zu 6 PIDs in einer Anfrage (`010C0D0B11`), Antwort-Parser dafür.
- ELM-Init: `ATZ, ATE0, ATL0, ATS0, ATH0, ATSP0, ATAT2`. Timeout und Neuverbindung mit wachsender Pause (1, 2, 5, 10 s).
- Anzeige "Abfragen pro Sekunde" im Diagnose-Bereich.

### Verbrauchsberechnung
Reihenfolge, die erste verfügbare Quelle gewinnt:
1. PID 0x5E (Fuel Rate, l/h)
2. MAF (0x10): `Kraftstoff g/s = MAF / AFR`
3. **Speed-Density über MAP** (für den Modus):
   `Luft g/s = MAP[kPa] · Hubraum[l] · Drehzahl/120 · VE / (0,28705 · IAT[K])` (ideales Gasgesetz, R_Luft = 0,28705 kJ/(kg·K); Viertakter saugt je zwei Umdrehungen einmal den Hubraum an)
   `Kraftstoff g/s = Luft / AFR · (1 + (STFT+LTFT)/100)`
   `l/h = Kraftstoff g/s · 3600 / Dichte[g/l] · fuel_cal`
- **Konstanten:** Benzin AFR 14,7, Dichte 745 g/l, Heizwert 32 MJ/l. Diesel Dichte 832 g/l, Heizwert 36 MJ/l. Liefert das Auto das Soll-Lambda (PID 0x44), gilt `AFR = 14,7 · λ_soll`; so wird die Anfettung bei Volllast mitgerechnet. Die Gemischkorrektur (STFT/LTFT) gilt auch für den MAF-Weg.
- **Diesel** ohne 0x5E: Luftmasse allein reicht nicht (Diesel fährt mager mit wechselndem λ). Nur mit Breitband-Lambda (PIDs 0x34–0x3B) rechenbar, sonst zeigt die Firmware "Verbrauch nicht verfügbar". Für den Modus-Benziner nicht relevant.
- Prüfwert Modus 1.2 im Leerlauf: 32 kPa, 780 U/min, VE 0,85, 25 °C → 2,4 g/s Luft → 0,16 g/s Benzin → 0,78 l/h. Plausibel (typisch 0,6–0,9 l/h).
- **Schubabschaltung:** Status PID 0x03 Wert 4 = "open loop due to deceleration" (oder Drosselklappe ≈ 0 % bei > 1200 U/min und Tempo > 15 km/h) → Verbrauch exakt 0. Das ist für den Eco-Modus zentral.
- **Selbstkalibrierung:** Nur zwischen zwei Vollbetankungen ist bekannt, wie viel wirklich verbraucht wurde: `Verhältnis = getankte Liter (Summe aller Füllungen seit der letzten Vollbetankung, inklusive dieser) ÷ berechnete Liter im selben Zeitraum`. Gilt nur, wenn mindestens 150 km gefahren wurden und das Verhältnis zwischen 0,7 und 1,3 liegt (sonst Tippfehler oder Fehlbetankung). Dann `fuel_cal_neu = fuel_cal_alt · √Verhältnis` (halbe Korrektur je Füllung, dämpft Ausreißer), Grenzen 0,7–1,3. Nach 2–3 Vollbetankungen mit Beleg-Litern ist die Anzeige typischerweise auf ± 3 % genau.
- l/100 km = l/h / km/h · 100, nur ab 5 km/h; darunter zeigt die Firmware l/h.

### Mittelwerte über Strecke
- **Momentan:** 1-s-Mittel.
- **1 km (letzter Kilometer):** Ringpuffer mit 20 Abschnitten à 50 m.
- **10 km:** Ringpuffer mit 100 Abschnitten à 100 m (Strecke und Liter je Abschnitt).
- **100 km:** Ringpuffer mit 100 Abschnitten à 1 km.
- **Tank** (erster Punkt der Eco-Kurve, Jos Wunsch vom 3. Oktober): Schnitt seit der letzten Tankfüllung. In den ersten 30 km nach dem Tanken gilt noch der Schnitt der vorigen Füllung, sonst würde der Wert stark springen. Bewusst nicht "letzte 1000 km": der Tank-Schnitt passt zu Spar-Ziel, "Ø / Ziel" und Tankliste und ist für den Fahrer nachvollziehbar.
- **Seit Profilanlage:** Summe km und Liter seit Profilanlage bzw. Reset; nur noch in Historie → Auswertung und als wählbare Kachel "Ø seit Profil".
- Zusätzlich: aktuelle Fahrt, aktuelle Tankfüllung.
- **Die Ringpuffer sind echte Strecken-Fenster**, keine gleitenden Näherungen: Jeder Abschnitt speichert gefahrene Meter und verbrauchte Milliliter, der Schnitt ist Summe Liter ÷ Summe km. Nur ein angebrochener Abschnitt läuft live mit.
- **Sie überleben Stromausfall und Abstellen:** Alle Puffer, die Gesamtsummen und der Tankinhalt werden alle 60 s und bei jedem Halt gespeichert (Kapitel 8) und beim Start geladen. Ø 100 km nach dem Wiedereinsteigen ist also derselbe Wert wie beim Abstellen; verloren geht höchstens die letzte Minute vor dem Stromausfall. Der Momentanwert startet neu.
- **Genauigkeit:** Die Strecke kommt aus dem OBD-Tempo (meist 1–3 % genau, einstellbarer km-Faktor im Profil, mit GPS automatisch). Der Verbrauch ist vor der ersten Kalibrierung über den Saugrohrdruck auf etwa ± 10–15 % genau, nach zwei bis drei Tankfüllungen mit Beleg-Litern auf etwa ± 3 %. Alle Fenster nutzen dieselbe Rechnung, ihre Verhältnisse zueinander stimmen also auch vorher.

### Restreichweite
- Mit PID 0x2F: Tankinhalt = Füllstand · Tankgröße, geglättet.
- Ohne 0x2F (wahrscheinlich beim Modus): **Tankmodell**. Nach einer Vollbetankung ist der Tank voll (Tankgröße aus dem Profil), nach einer Teilbetankung gilt Rest + getankte Liter (höchstens Tankgröße). Danach wird der berechnete Verbrauch abgezogen.
- **Prognose-Verbrauch** = 50 % Ø 100 km + 30 % Ø 10 km (aktueller Fahrstil, z. B. Autobahn) + 20 % Ø der letzten 5 Tankfüllungen (Historie dieses Autos). Fehlen Daten, werden die Gewichte auf die vorhandenen verteilt.
- Reichweite = Rest-Liter ÷ Prognose-Verbrauch · 100. Geglättet wird nur der Prognose-Verbrauch (gleitender Mittelwert, Zeitkonstante 60 s), nicht die Reichweite selbst: Gefahrene Kilometer gehen also sofort ab, und nach dem Tanken springt die Reichweite sofort auf den neuen Wert.
- **Genauigkeit:** typisch ± 5–10 %. Die größte Unsicherheit ist der Rest im Tank (Tankmodell oder grober Tankgeber), nicht die Prognose. Unter 80 km rechnet die Firmware vorsichtig mit dem höheren der drei Schnitte; unter 50 km wird die Kachel bernsteinfarben.
- Darstellung überall gleich (Übersicht, Großanzeige, Fahrt & Tank): Reichweite groß, darunter geteilter Balken gefahren | Rest und die Zeile "⛽ ··· 316" (km seit Tanken) und "Σ 763 km" (gefahren + Reichweite), siehe UI-Entwurf.

### Tankfüllung und automatische Tankerkennung
**Automatisch geht es nur, wenn das Auto den Tankfüllstand (PID 0x2F) liefert.** Ob der Modus das tut, zeigt die erste Verbindung (Menü → Diagnose → unterstützte PIDs). Ohne Füllstand kann die Firmware einen Tankvorgang nicht bemerken, dann bleibt der Knopf "Getankt".

Erkennung mit PID 0x2F:
1. Beim Motorstart den Füllstand über 10 s mitteln (die Anzeige schwappt, deshalb nur im Stand und gemittelt).
2. Vergleich mit dem gespeicherten Füllstand vom letzten Abstellen. Anstieg um mindestens 8 % der Tankgröße (beim Modus ca. 4 l) gilt als Tankvorgang.
3. Getankte Liter = Anstieg × Tankgröße. Genauigkeit: Tankgeber sind grob, typisch ± 2–3 l. Liegt der Tank nach dem Tanken bei über 95 %, nimmt die Firmware stattdessen die berechneten Liter seit der letzten Vollbetankung (genauer).
4. Das **Tank-Fenster** öffnet sich sofort über der aktuellen Seite (globales Overlay in LVGL auf `lv_layer_top()`), egal in welchem Modus.

Tank-Fenster (siehe UI-Entwurf):
- Links die getankten Liter (Format 00,0 + Tropfen-Symbol), rechts der Preis pro Liter (0,00 + hochgestellte ⁹ + €), vorausgefüllt mit dem letzten Zapfsäulenpreis. Jede Stelle hat ein ▲ darüber und ein ▼ darunter, Überlauf rechnet weiter; gedrückt halten wiederholt. Tippen auf die Ziffern öffnet ein Ziffernfeld. Kosten werden live mitgerechnet (Liter × Preis inkl. ⁹).
- Schalter **"vollgetankt"** (vorgewählt) bzw. "nicht voll": steuert das Tankmodell und ob kalibriert wird (siehe oben).
- OK speichert, "Nicht getankt" verwirft.
- Ohne PID 0x2F: gleicher Dialog über "Getankt", Liter = berechneter Verbrauch seit der letzten Füllung (Annahme vollgetankt).
- **Liter im Dialog direkt korrigierbar** (▲/▼ für 10 l, 1 l und 0,1 l, Tippen = Ziffern). Beleg-Liter sind exakt und kalibrieren `fuel_cal`. Genauigkeit der Vorschläge: über Tankanzeige ± 2–3 l, aus berechnetem Verbrauch ± 10–15 % vor und ± 3 % nach der Kalibrierung. Empfehlung: die ersten drei Tankfüllungen die Liter vom Beleg eintragen, danach reicht meist OK.
- Gespeichert werden km-Stand der Fahrten, Liter, Preis, Kosten, Ø-Verbrauch, Kosten pro 100 km und, mit GPS, Datum. Diese Werte gelten für die Kosten aller Fahrten der Tankfüllung.
- **Mischpreis** (Jos Frage vom 3. Oktober): Im Tank mischen sich Reste alter Füllungen mit neuem Sprit. Nach jedem Tanken gilt `Mischpreis = (Restliter × alter Mischpreis + getankte Liter × neuer Preis) ÷ (Restliter + getankte Liter)`. Restliter kommen aus dem Füllstand (PID 0x2F) bzw. aus dem Tankmodell. Alle Fahrtkosten werden mit dem Mischpreis gerechnet; Fahrt & Tank zeigt ihn als "Ø-Preis im Tank". Das Tank-Fenster schlägt dagegen den zuletzt eingegebenen Zapfsäulenpreis vor. Eingegeben werden Euro und Cent, die Firmware hängt die 9/10 Cent der Zapfsäule immer an (Eingabe 1,79 → 1,799 €/l, angezeigt als 1,79⁹). Gespeichert und gerechnet wird mit dem vollen Preis. Beispiel: 15 l Rest zu 1,799 € + 30 l zu 1,699 € → 1,732 €/l.
- Der Mischpreis gilt überall, wo Sprit verfahren wird: Kosten der laufenden Fahrt, Fahrtdatensatz (Kosten werden beim Speichern festgeschrieben, nicht später mit neuem Preis umgerechnet), Start-Karte, Historie-Summen. Nur das Tank-Fenster nutzt den letzten Zapfsäulenpreis als Vorschlag, und die Tankliste zeigt je Füllung den bezahlten Preis.
- **Warum nicht FIFO** (erst den alten Sprit abrechnen, dann den neuen): Im Tank mischt sich der Sprit tatsächlich, die Summe aller Kosten ist bei beiden Verfahren gleich, nur die Verteilung auf einzelne Fahrten verschiebt sich um wenige Cent. FIFO bräuchte eine Liste von Tankschichten und wäre nach einer Fehlerkennung schwer zu korrigieren. Der Mischpreis ist einfacher, robuster und das übliche Verfahren (gewogener Durchschnitt).

## 8. Speichern ohne Datenverlust

Der Strom geht mit der Zündung schlagartig weg. Für den ESP32 selbst ist das unschädlich, gefährlich ist nur ein Schreibvorgang in den Flash, der mittendrin abbricht. Deshalb:
- **Dateisystem LittleFS** ist für Stromausfall gebaut: Es schreibt neue Daten an eine freie Stelle und schaltet erst danach um (copy-on-write). Eine abgebrochene Datei ist danach die alte Version, nie ein kaputtes Dateisystem. Einstellungen liegen in NVS, das ebenfalls stromausfallsicher ist.
- **Brownout-Erkennung an** (Standard im ESP-IDF): Sinkt die Spannung beim Abschalten langsam, setzt sich der Chip sauber zurück, statt mit halber Spannung weiterzulaufen.
- **Kein Schreiben auf die microSD im Fahrbetrieb**, nur beim Export auf Knopfdruck (FAT auf SD ist nicht stromausfallsicher).
- **Flash-Verschleiß:** Speichern alle 60 s ergibt bei 1 h Fahrt am Tag etwa 22 000 kleine Schreibvorgänge im Jahr. LittleFS verteilt sie über die 4-MB-Partition; das hält weit über die Lebensdauer des Geräts.
- Laufende Summen (Ringpuffer, Tankmodell, Fahrt, letzter Tankfüllstand) alle 60 s und bei jedem Stillstand > 10 s nach LittleFS schreiben, abwechselnd in zwei Dateien (A/B mit Prüfsumme). Beim Start gilt die jüngste gültige Datei.
- **Fahrt-Ende ohne Uhr:** Während der Strom weg ist, kann das Board keine Zeit messen. Deshalb entscheidet beim Start:
  1. Mit GPS-Uhrzeit: Pause > 5 min → neue Fahrt.
  2. Ohne GPS über die Kühlmitteltemperatur: War der Motor beim Abstellen warm (≥ 70 °C) und ist er beim Start höchstens 4 °C kälter, war es eine kurze Pause (Tanken, Bäcker) → Fahrt läuft weiter. Sonst neue Fahrt.
  3. Bleibt der USB-Strom bei Zündung aus an (manche Autos), misst die Firmware die Zeit selbst: kein Motorsignal länger als 5 min → Fahrt beenden.
  4. Oder Jo beendet sie im Menü.
  Die beendete Fahrt wandert ins Fahrtenbuch (50 Einträge, älteste fliegt raus).
- **microSD nur beim Export:** Menüpunkt "Fahrten exportieren" schreibt Fahrtenbuch und Tankfüllungen als CSV. Sonst wird nie auf die Karte geschrieben.

### Historie-Datensätze (LittleFS)
- **Fahrt** (letzte 50, je ca. 64 Byte): Nummer, Datum (nur mit GPS), km, Liter, Dauer, Leerlaufzeit, Schubzeit, Gebremst (l), Eco-Score, max. Drehzahl, max. Kühlmittel, Kosten, Profil.
- **Tankfüllung** (letzte 100): Nummer, Datum (GPS), km seit letzter Füllung, Liter (erkannt/berechnet/korrigiert), Preis, Ø, Kosten pro 100 km.
- Auswertungen (Verbrauch nach Streckenlänge, Trends, Bestwerte) werden beim Öffnen der Seite aus diesen Datensätzen berechnet, nichts davon wird extra gespeichert.

## 9. Eco-Logik (Hypermiling)

| Funktion | Berechnung |
|---|---|
| Verbrauchsfarbe | Momentanverbrauch und die Punkte der Eco-Kurve im Verhältnis zum Spar-Ziel, ohne Ziel zum Tank-Schnitt: `good` unter 95 %, neutral 95–110 %, `warn` über 110 %. Dieselben Schwellen gelten für die Fahrten-Balken in der Historie (Bezug dort der Gesamtschnitt) |
| Schub-Anzeige | Schubabschaltung aktiv → "SCHUB 0,0 l" groß. Zähler "Schub gespart" = Schubzeit × gelernter Leerlaufverbrauch (l/h im Stand, warm), also der Sprit, den ausgekuppeltes Rollen gekostet hätte |
| Gang + Schaltempfehlung | Gang aus gelernter Übersetzung. Hochschalten empfohlen ab `shift_rpm` (Benziner 2200, Diesel 1800) bei Gaspedal < 60 %, nicht im höchsten Gang, und nur wenn die Drehzahl im nächsten Gang (`Drehzahl · k_Gang ÷ k_Gang+1`) noch mindestens 1300 U/min (Diesel 1200) beträgt; so wird kein Untertouren empfohlen. Ohne gelernte Gänge nur nach Drehzahl |
| Gaspedal-Ruhe | Änderungsrate des Gaspedals; ruhig = gut. Fließt in den Eco-Score |
| Leerlauf | Zeit und Liter im Stand in dieser Fahrt |
| Eco-Score 0–100 | je Teil 0–100 Punkte, gewichtet: Schub und Rollen (Anteil der Fahrzeit in Bewegung mit Schubabschaltung oder Segeln ohne Verzögerung > 0,5 m/s²) 35 %, ruhiges Gaspedal 25 %, früh schalten (Anteil Fahrzeit mit offener Schaltempfehlung) 20 %, sanft bremsen (Gebremst-Liter je 100 km) 10 %, wenig Leerlauf (Anteil Standzeit mit laufendem Motor an der Fahrzeit) 10 %. Jeder Teil wird auf 0–100 begrenzt, der Score ist die gewichtete Summe, gerundet. Bewertung: ab 80 gut (`good`), 60–79 ok (neutral), darunter Luft nach oben (`warn`), überall gleich eingefärbt. Normierung der Teile: Rollen 25 % der Fahrzeit (in Bewegung) = 100 Punkte; ruhiges Gas 100 − 25 × (mittlere Änderung des geglätteten Pedals in %/s − 1); früh schalten 100 − 400 × Anteil Zeit mit offener Schaltempfehlung; sanft bremsen 100 bei 0 l/100 km, 0 ab 1,5 l/100 km; Leerlauf 100 bei 0 %, 0 ab 30 % Standzeit. Je Fahrt gespeichert, Erklärung auf der Info-Seite |
| Kalter Motor | Kühlmittel < `cold_coolant_c` (60 °C) und Drehzahl > Kalt-Grenze (Menü, 2000–3000 U/min, Standard 2500) → Drehzahl-Kachel und Hinweis-Feld `warn` (Q13d) |

### Spartipps (Regelwerk)
Kurze Einblendung im Hinweis-Feld der Eco-Seite links neben der Ganganzeige (Jos Wunsch vom 3. Oktober, nicht mehr unter der Kurve): Symbol + höchstens vier Wörter auf zwei Zeilen, 8 s sichtbar, dann weg. Jeder Hinweis verschwindet sofort, wenn sein Anlass vorbei ist: "Gang rein", sobald ein Gang drin ist oder nicht mehr gebremst wird; Stand, sobald das Auto fährt. Während Schubabschaltung erscheinen keine Hinweise. Höchstens ein Tipp gleichzeitig, mindestens 60 s Abstand zwischen zwei Tipps, derselbe Tipp frühestens nach 5 min wieder. Ausnahme Stand-Hinweis: kommt alle 30 s wieder, solange das Auto steht, mit mitlaufender Standzeit. Abschaltbar im Menü. Ist kein Tipp aktiv und der Motor kalt (Regel Kalter Motor), zeigt das Feld ein Thermometer-Symbol, daneben "Motor 57 °C" und darunter kleiner "max. 2.500 U/min" (Kalt-Grenze aus dem Menü) in `warn`, solange die Bedingung gilt; das ist auch bei abgeschalteten Spartipps aktiv. Ohne Anlass bleibt das Feld leer.

**Schalthinweis nur als Pfeil** (Jos Wunsch vom 3. Oktober): grüner ▲ vor der Gangzahl, sobald die Schaltempfehlung 1 s anliegt, und weg, sobald hochgeschaltet ist. Kein Text, keine Sperrzeit.

| Tipp | Bedingung (Firmware-Werte, im Profil einstellbar) | Text |
|---|---|---|
| Gang rein (statt ausgekuppelt bremsen) | Ausgekuppelt (Drehzahl im Leerlauf, Tempo > 20 km/h) **und** Verzögerung > 0,5 m/s² seit 2 s, also deutlich mehr als natürliches Ausrollen (≈ 0,3 m/s² bei 80 km/h). Segeln, das das Tempo hält oder bergab sogar steigert, ist sparsam und löst keinen Hinweis aus (Jos Hinweis vom 3. Oktober); es zählt im Eco-Score wie Schub als "Rollen". Nur wer ausgekuppelt verzögert, verschenkt Schub (0 l) gegen Leerlaufsprit | 0 l · Gang rein · Schub 0 l |
| Sanfter Gas geben | Gaspedal > 70 % seit > 3 s bei Tempo > 30, kein Überholvorgang erkennbar (mit MPU: Längsbeschleunigung hoch) | ▼ Sanfter Gas geben |
| Früher vom Gas | Starke Verzögerung (> 2,5 m/s² aus Tempo bzw. MPU) innerhalb von 4 s nach Gaspedal > 30 % | → Früher vom Gas |
| Langer Stand | Motor läuft, Tempo 0 seit > 60 s; danach alle 30 s erneut, solange das Auto steht | P · Stand 1:30 · Motor aus? |
| Tempo | > 115 km/h seit > 30 s; Text nennt die Ersparnis aus der eigenen Spartempo-Statistik (Klassen 100 und 120 je ≥ 5 km), ohne diese Daten kein Tipp | 100 statt 120: −1,2 l |
| Gleichmäßig fahren | Tempo konstant (± 3 km/h) aber Gaspedal schwankt stark (Standardabweichung > 8 %) über 10 s | ≈ Gleichmäßig Gas halten |
| Kalter Motor | Kühlmittel < 60 °C und Drehzahl > 2500, im Hinweis-Feld solange die Bedingung gilt, ohne Sperrzeit | 🌡 Motor 57 °C / max. 2.500 U/min |
| Thermostat (einmalig, `warn`) | Fahrzeit > 15 min, davon > 8 min über 50 km/h, Kühlmittel < 75 °C, Ansaugluft beim Start > −5 °C. Danach Eintrag auf der Seite Fehlercodes. Höchstens alle 10 Starts | ! Motor bleibt kalt · Thermostat? |

Sicherheitsregel: Keine Tipps unter 3 s nach einem Tippen aufs Display, und keine Tipps, solange ein Dialog offen ist.

### Zusatzfunktionen (Jos Auswahl vom 3. Oktober)
Ort, Darstellung und Logik stehen in `zusatzfunktionen.md`. Kurzfassung für die Firmware:
- **Bremsenergie:** `P_brems = max(0, −m·a − F_luft − F_roll [− F_hang mit MPU]) · v` mit `F_luft = ½ · 1,2 · cw·A · v²`, `F_roll = 0,012 · m · 9,81`, nur wenn nicht Gas gegeben wird; aufsummiert je Fahrt, Liter = J ÷ (0,25 · Heizwert), Benzin 32 MJ/l, Diesel 36 MJ/l (0,25 = typischer Motorwirkungsgrad: so viel Sprit hat die Bewegungsenergie gekostet). Das Schleppmoment des Motors im Schub ist nicht bekannt und wird mitgezählt; der Wert ist also eine Obergrenze. Kachel "Gebremst" auf der Eco-Seite.
- **Spar-Ziel:** NVS `goal_mode` (aus / auto / fest) und `goal_l100` (fest 3,0–7,0 in 0,1-Schritten). Auto = Ø der letzten fünf Tankfüllungen (ersatzweise Gesamt) minus 0,5 l, wenn dieser über 5 l liegt, sonst minus 0,3 l; mindestens 3,0. Auto wird bei jedem Tanken neu gesetzt. Ersetzt in der Eco-Kurve die Tank-Schnitt-Linie und dient als Bezug der Punktfarben.
- **Spartempo:** 11 Klassen 30–130 km/h, je Klasse km und Liter in NVS. Zählt nur bei höchstem Gang, Tempo ± 3 km/h über 20 s, Steigung < 1 % (MPU) bzw. ruhigem Gaspedal. Klasse gilt ab 5 km.
- **Start-Karte:** aus dem letzten Fahrtdatensatz, wenn ≥ 1 km. 6 s, Tippen oder Tempo > 5 km/h schließt.
- **Wartung:** NVS `odo_km` (einmal Tachostand eintragen, danach eigene Zählung), `oil_due_km`, `insp_due_km`. Zeile in der Start-Karte bei < 500 km.
- **Sonnenstand:** NOAA-Näherung aus GPS-Position und Datum, 15 min Übergang. Nur wenn GPS-Fix.

## 10. Sport und Sprint (zwei Seiten)
Neu gestaltet nach Jos Wunsch vom 3. Oktober (mehr Live-Anzeigen und Diagramme).

**Seite Sport (live)**
- Drehzahlmesser als 240°-Bogen 0–6500 U/min (Skala ×1000, ab 5800 `warn`, Rotbereich-Grenze im Profil), in der Mitte Tempo groß und Gang mit Schaltpfeil ▲.
- Vier Live-Balken: geschätzte Leistung (kW · PS, Balken bis Nennleistung), Beschleunigung in m/s² als Balken um die Mitte (links bremsen, rechts beschleunigen; mit MPU6050 Längs-G), Gaspedal %, Saugrohrdruck kPa.
- Live-Diagramm der letzten 30 s aus einem eigenen Ringpuffer mit jeder Abfrage (ca. 4–8 Hz, 240 Einträge). Antippen der Legende schaltet das Serienpaar: Tempo + Leistung, Drehzahl + Gas, Beschleunigung + Leistung. Feste Achsen je Serie (Tempo 0–140, kW 0–60, U/min 0–6000, Gas 0–100, Beschleunigung −4…+4 m/s²). Die Wahl wird gespeichert.
- Läuft eine Sprintmessung, steht oben im Diagramm "0–100 läuft · 3,8 s".

**Seite Sprint**
- Stoppuhr 0–100 mit Status bereit / läuft / geschafft, Fortschrittsbalken.
- Tempoverlauf der letzten Messung (`accent`) und der besten (`good`, gestrichelt), aufgezeichnet mit jeder Abfrage während der Messung, beste Kurve je Profil gespeichert.
- Tabelle 0–50, 0–100, 80–120 km/h mit letzter und bester Zeit.
- Fahrtwerte als vier Kacheln: Ø Fahrt, Zeit pro 100 km (= 100 ÷ Ø Fahrt), Vmax, Spitzenleistung.
- **Auto-Sprint (Menü, Standard an):** Die Firmware erkennt einen Sprint, wenn das Auto mindestens 1 s stand und dann, spätestens 3 s nach dem Anfahren, das Gaspedal (PID 0x49, sonst 0x11) bei mindestens 85 % und die Drehzahl bei mindestens 3000 U/min liegt. Dann wechselt die Anzeige zur Sprint-Seite und merkt sich die vorige Seite. Zurück geht es 4 s nach dem Ziel (100 km/h), 2 s nachdem das Gaspedal unter 50 % fällt, sobald die Messung nicht läuft (3 s nach dem Wechsel) oder spätestens nach 25 s. Wischt der Fahrer selbst oder öffnet ein Fenster, entfällt der Rücksprung. Ist ein Fenster offen, wird nicht gewechselt. Normales Anfahren (Gaspedal um 40–50 %, Schalten unter 3000 U/min) löst nichts aus.
- **Messung 0–50 / 0–100:** Aufzeichnung startet, sobald das Auto losrollt (Startzeit = Zeitpunkt, an dem das Tempo 0 verlässt, zwischen zwei Abfragen linear interpoliert). Gültig wird sie erst, wenn innerhalb von 3 s ein Sprint erkannt wird (Regel Auto-Sprint); normales Anfahren wird still verworfen und erscheint nie in der Tabelle. Zielzeiten ebenfalls interpoliert. Abbruch, wenn das Gaspedal länger als 1,5 s unter 50 % liegt (ein Schaltvorgang dauert kürzer und bricht nicht ab) oder das Tempo um mehr als 3 km/h fällt.
- **80–120:** startet beim Durchfahren von 80 km/h mit Gaspedal ≥ 85 %, gleiche Abbruchregel.
- Während einer Messung fragt der Scheduler nur Tempo, Drehzahl und Gaspedal ab (höherer Takt). Genauigkeit mit OBD allein ca. ± 0,2 s, mit MPU6050 oder GPS (10 Hz) ± 0,05 s.
- Beste Zeiten und beste Kurve je Profil in NVS.

**Leistung (geschätzt)**
- `P = (m · a + ½ · ρ · cw·A · v² + c_r · m · g) · v` mit ρ = 1,2 kg/m³, Leistung am Rad (ohne Getriebeverluste, deshalb etwas unter der Nennleistung). Prüfwert: 50 km/h, 2 m/s², 1150 kg → 35 kW. Gewicht und cw·A aus der Fahrzeugart (Kleinwagen 1150 kg, 0,70 m²), c_r 0,012. a aus der Tempoänderung (gefiltert), mit MPU6050 gemessen. Genauigkeit etwa ± 15 %.

### Beschleunigungssensor MPU6050: Einbaulage, Steigung, Kalibrierung
- **Was er misst:** Beschleunigung in drei Achsen (inklusive Erdanziehung) und Drehrate in drei Achsen (Gyroskop), 100-mal pro Sekunde.
- **Einbaulage lernen (einmalig, automatisch):** Das Board kann schräg oder gedreht montiert sein.
  1. Auto steht auf ebener Fläche, Motor aus oder Leerlauf: Die Firmware misst 3 s lang die Schwerkraft. Ihre Richtung ist "unten".
  2. Bei der ersten geradeaus Beschleunigung aus dem Stand (OBD-Tempo steigt, Gyro dreht kaum) zeigt der Rest der Beschleunigung nach "vorne". Daraus wird eine Drehmatrix berechnet und im Profil gespeichert.
  3. Menüpunkt "Sensor neu einlernen", falls das Display umgebaut wird.
- **Steigung vs. Beschleunigung:** Der Sensor misst längs `a_Sensor = a_echt + g · sin(Steigung)`. Das OBD-Tempo liefert `a_echt` (Ableitung der Geschwindigkeit, langsam, aber frei von Steigung). Ein Filter (komplementär bzw. kleiner Kalman) schätzt die Steigung aus der langsamen Differenz beider Werte und nutzt die Nick-Drehrate des Gyros für schnelle Änderungen. Ergebnis: schnelle, steigungsbereinigte Beschleunigung plus Steigung in %.
- **Nutzen der Steigung:** Leistungsschätzung rechnet die Hubarbeit mit ein (bergauf sonst zu niedrig), Spartipps unterdrücken "Sanfter Gas geben" am Berg, Verbrauch kann nach Steigung ausgewertet werden.
- **Laufende Nachkalibrierung:** Bei jedem Stillstand über 3 s wird der Gyro-Nullpunkt neu gemittelt (Drift), und der Schwerkraftvektor wird mit der gelernten Lage verglichen; die Abweichung ist die Steigung des Standplatzes, nicht ein Fehler.
- **Temperatur:** Der MPU6050 driftet mit der Temperatur; der eingebaute Temperaturfühler geht in eine einfache lineare Korrektur ein.
- **Ohne Sensor:** Beschleunigung nur aus dem OBD-Tempo (grob, wenige Werte pro Sekunde), keine Steigung.

## 11. Fehlercodes
- Mode 03 (gespeichert) und 07 (vorläufig). Klartext aus einer eingebauten Tabelle der häufigsten generischen P0-Codes auf Deutsch (im Flash, ca. 300 Einträge), sonst "Herstellerspezifischer Code".
- Löschen (Mode 04): nur wenn Drehzahl = 0, Sicherheitsabfrage "Fehlercodes wirklich löschen? Die Motorkontrollleuchte geht aus, Lernwerte werden zurückgesetzt".
- Kopfzeile zeigt ein kleines Motor-Symbol, wenn die MIL an ist.

## 12. Dateien (Zielstruktur)

```
car-display-v2/
├─ platformio.ini            Umgebungen: freenove, simulator
├─ include/
│  ├─ board_fnk0104b.h       alle Pins (siehe 3)
│  ├─ config.h               Schwellen, Defaults
│  └─ lv_conf.h
├─ src/
│  ├─ main.cpp               Tasks starten
│  ├─ core/car_state.h       CarState, Befehls-Queue
│  ├─ obd/ble_link.*         NimBLE-UART zum Adapter
│  ├─ obd/elm327.*           Init, Befehle, Antwort-Parser, Multi-PID
│  ├─ obd/pid_scheduler.*    Takt-Klassen, unterstützte PIDs
│  ├─ obd/dtc.*              lesen, löschen, Klartext
│  ├─ calc/fuel.*            Verbrauch, Kalibrierung, Tankmodell
│  ├─ calc/averages.*        Ringpuffer 10/100 km, Gesamt
│  ├─ calc/eco.*             Score, Schub, Gang, Schaltempfehlung
│  ├─ calc/trip.*            Fahrt, Fahrtenbuch, Tankfüllungen
│  ├─ calc/perf.*            0–100, 80–120
│  ├─ sensors/gps.*, imu.*   optional
│  ├─ storage/store.*        LittleFS A/B, NVS-Einstellungen, Profile
│  ├─ sim/simulator.*        simulierte Fahrt (Stadt, Land, Schub, Leerlauf, Tanken)
│  └─ ui/                    theme, statusbar, je Seite eine Datei, dialogs
└─ data/dtc_de.csv           Klartext-Tabelle
```

## 13. Simulator
`pio run -e simulator -t upload` ersetzt `obdTask` durch eine aufgezeichnete Beispielfahrt (Stadt, Landstraße, Schubphasen, Ampel-Leerlauf, Kaltstart, ein Fehlercode, Tanken). Damit lässt sich die komplette Oberfläche ohne Auto prüfen.
