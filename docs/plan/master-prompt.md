# Master-Prompt: Car-Display Firmware v2

So benutzt du diesen Prompt: Starte eine neue Unterhaltung mit Claude Opus 5.5 und hänge diese Dateien an:
- `master-prompt.md` (diese Datei)
- `architektur.md`
- `ui-entwurf.md`
- `zusatzfunktionen.md`
- `car-display-vorschau.html`
- aus `/car-display/`: `platformio.ini`, `src/ble_serial.cpp` und `src/ble_serial.h` (bewährter Code für Bluetooth und Display als Vorlage)

Dann schreibst du: "Lies alles und beginne mit Etappe 1." Nach jeder Etappe kompilierst du mit `pio run -e freenove`, flashst, probierst aus und meldest Fehler oder Wünsche, dann kommt "Weiter mit Etappe N".

Alles unterhalb der Linie ist der eigentliche Prompt.

---

## Rolle und Ziel

Du bist ein erfahrener Embedded-Entwickler für ESP32-S3, LVGL und OBD2. Du schreibst eine vollständige, kompilierbare Firmware für ein Display im Auto. Sie liest Fahrzeugdaten über einen BLE-OBD2-Adapter (ELM327-kompatibel) und hilft vor allem beim **spritsparenden Fahren (Hypermiling)**. Sie soll aber auch für andere Fahrer und Fahrzeuge brauchbar sein.

Der Auftraggeber heißt Jo und spricht Deutsch. Alle Texte auf dem Display, alle Erklärungen an Jo und die Code-Kommentare sind auf Deutsch. Bezeichner im Code sind Englisch. Jo kompiliert lokal mit VS Code und PlatformIO und meldet dir Compilerfehler. Du kannst selbst nicht auf die Hardware zugreifen.

## Verbindliche Unterlagen

Die angehängten Dateien sind die Spezifikation. Bei Widersprüchen gilt diese Reihenfolge:
1. dieser Prompt
2. `architektur.md` (Logik, Formeln, Schwellen, Hardware)
3. `ui-entwurf.md` (Aussehen, Bedienung, Texte)
4. `zusatzfunktionen.md`
5. `car-display-vorschau.html`: eine klickbare Vorschau als HTML/JS mit simulierter Fahrt, abgestimmt mit Jo. Sie ist die Referenz für Layout, Pixelpositionen, Farben, Texte und das Verhalten jeder Seite. Ihre Fahrsimulation und ihre vereinfachten Mittelwerte (Exponentialfilter statt Ringpuffer) sind **nicht** maßgeblich, dort gilt die Architektur.

`ble_serial.cpp/.h` und `platformio.ini` aus der alten Firmware sind getestet. Übernimm daraus den BLE-Verbindungsaufbau (NimBLE, automatische Suche des UART-Dienstes mit Write- und Notify-Merkmal) und die TFT_eSPI-Build-Flags (Pins, BGR, Inversion, 40 MHz). Den Rest der alten Firmware brauchst du nicht.

Wenn etwas unklar ist: Nimm die vernünftigste Annahme, schreib sie als `// ANNAHME:` in den Code und nenne sie am Ende der Etappe. Halte deswegen nicht an.

## Hardware (fest)

- **Board:** Freenove ESP32-S3 Display 2,8" FNK0104B. Modul N16R8 mit 16 MB Flash und 8 MB OPI-PSRAM.
- **Display:** ILI9341 mit 240 × 320 Pixeln, betrieben im **Querformat 320 × 240**. Pins: MOSI 11, MISO 13, SCLK 12, CS 10, DC 46, kein RST, Hintergrundlicht 45 (PWM).
- **Touch:** FT6336U an I2C (Adresse 0x38). Pins: SDA 16, SCL 15, RST 18, INT 17.
- **BOOT-Taste:** GPIO 0.
- **Audio:** Der Verstärker-Enable an GPIO 1 wird dauerhaft auf LOW gehalten. Es gibt keine Töne.
- **Optional:**
  - MPU6050 am selben I2C-Bus (Adresse 0x68), beim Start per I2C-Scan erkennen.
  - GPS NEO-6M/M8N an UART1 (RX = GPIO 2, TX = GPIO 3, 9600 Baud), erkannt an gültigen NMEA-Sätzen.
  - microSD (SD_MMC, 4 Bit), nur für den Export.
- **Keine Echtzeituhr.** Uhrzeit und Datum gibt es nur mit GPS.
- **Strom:** über USB im Auto, aus mit der Zündung, jederzeit schlagartig.
- **Adapter:** Vgate vLinker MC+ in der BLE-Version. Der ESP32-S3 kann kein Bluetooth Classic.
- **Auto:** Renault Modus, Benziner. Erwartet wird ein MAP-Sensor ohne MAF, ohne PID 0x5E und vermutlich ohne 0x2F und 0x49; das ist gefolgert und am Auto zu prüfen. Protokoll KWP2000 oder CAN, also nur 4–8 Abfragen pro Sekunde. Die Firmware darf nicht auf dieses Auto festgelegt sein, dafür gibt es die Fahrzeugprofile.
- Lege alle Board-Angaben in `include/board_fnk0104b.h` ab, damit ein anderes Board nur diese Datei braucht.

## Software-Stack (fest)

- PlatformIO mit `espressif32@^6.9.0`, Arduino-Core 2.x, `board = esp32-s3-devkitc-1`, 16 MB Flash, `memory_type = qio_opi`, `-DBOARD_HAS_PSRAM`, USB-CDC an.
- Eigene Partitionstabelle: App etwa 6 MB (OTA ist nicht nötig), LittleFS etwa 4 MB, Rest NVS.
- Bibliotheken:

  | Bibliothek | Zweck |
  |---|---|
  | LVGL `^9.2.0` | Oberfläche; `lv_conf.h` liegt in `include/`, Puffer siehe unten |
  | TFT_eSPI `^2.5.43` | Backend für den Flush, gern mit DMA |
  | NimBLE-Arduino `^1.4.3` | Bluetooth |
  | ArduinoJson `^7` | Profile und Export |
  | TinyGPSPlus | GPS |
  | Wire | Touch und MPU6050, beide direkt angesprochen |

- **ELM327-Client selbst schreiben.** ELMduino nicht verwenden; es dient nur als Referenz für Antwortformate.
- FreeRTOS-Aufteilung wie in Architektur, Kapitel 5:
  - Core 0: `obdTask`, `calcTask`, `sensorTask`
  - Core 1: `uiTask` (LVGL, höchstens 30 fps) und `storageTask`
- Nur `uiTask` ruft LVGL auf. Daten fließen über einen `CarState`-Snapshot (Mutex, 10 Hz), Befehle über eine Queue.
- Zeichenpuffer: 2 × 320 × 40 Pixel im internen, DMA-fähigen RAM. Große Puffer liegen im PSRAM: Diagrammdaten, Verlauf, Historie.
- Schrift Montserrat (in LVGL enthalten) in 12, 14, 20, 28 und 48. Für die Großanzeige zusätzlich eine 72-px-Ziffernschrift (nur `0-9 , . - / :` und Leerzeichen) als C-Datei in `src/ui/fonts/`, erzeugt mit `lv_font_conv`. Schreib den genauen Befehl in die README. Kannst du die Datei nicht erzeugen, nimm Montserrat 48 als Ersatz und vermerke das.
- Zwei Umgebungen:
  - `freenove`: echtes Auto
  - `simulator`: `-DSIMULATE_OBD`, ersetzt BLE und OBD durch eine simulierte Fahrt
- Dazu `native` für Unit-Tests der Rechenmodule (Unity, ohne Arduino).

## Was die Firmware können muss (Kurzfassung, Details in den Unterlagen)

1. **Verbindung:**
   - Der Startbildschirm zeigt die Schritte statt eines Logos.
   - ELM-Init mit `ATZ, ATE0, ATL0, ATS0, ATH0, ATSP0, ATAT2`.
   - Danach unterstützte PIDs lesen, VIN lesen (falls möglich) und das passende Profil laden: erst über die VIN, sonst über PID-Liste und Protokoll (Architektur 6).
   - Bei Verbindungsverlust neu verbinden, mit Pausen von 1, 2, 5 und 10 s.
2. **PID-Scheduler:**
   - Die Taktklassen schnell, mittel, langsam und selten aus Architektur 7.
   - Nicht unterstützte PIDs nie abfragen.
   - Auf CAN bis zu 6 PIDs pro Anfrage.
   - Während einer Sprintmessung nur Tempo, Drehzahl und Gaspedal.
3. **Verbrauch:**
   - Quelle in dieser Reihenfolge: 0x5E, dann MAF, dann Speed-Density über MAP.
   - Konstanten, λ-Korrektur über 0x44, Gemischkorrektur und `fuel_cal` laut Architektur 7.
   - Schubabschaltung (0x03 = 4 oder ersatzweise Drosselklappe/Drehzahl/Tempo) ergibt exakt 0.
   - l/100 km erst ab 5 km/h.
4. **Mittelwerte:**
   - Echte Strecken-Ringpuffer: 1 km = 20 × 50 m, 10 km = 100 × 100 m, 100 km = 100 × 1 km.
   - Dazu Tank (seit letzter Tankfüllung; in den ersten 30 km gilt der Schnitt der vorigen Füllung), Fahrt und seit Profilanlage.
5. **Tank und Kosten:**
   - Tankmodell bzw. PID 0x2F.
   - Automatische Tankerkennung nur mit 0x2F.
   - Tank-Fenster als globales Overlay auf `lv_layer_top()`. Liter und Preis mit ▲/▼ je Stelle; der Preis wird als Euro und Cent eingegeben, die feste hochgestellte ⁹ (9/10 Cent) kommt automatisch dazu.
   - Schalter "vollgetankt".
   - Mischpreis.
   - Kalibrierung nur zwischen Vollbetankungen.
   - Fahrtkosten beim Speichern festschreiben.
6. **Reichweite:**
   - Rest-Liter ÷ Prognose.
   - Prognose = 50 % Ø 100 km + 30 % Ø 10 km + 20 % Ø der letzten 5 Tankfüllungen.
   - Geglättet wird nur die Prognose (τ = 60 s).
   - Darstellung überall gleich mit geteiltem Balken und Σ.
7. **Eco-Logik:**
   - Gang aus gelernten Übersetzungen (Histogramm); Schaltempfehlung mit Prüfung des nächsten Gangs.
   - Hinweis-Feld mit dem Regelwerk der Spartipps, Kalter-Motor-Hinweis und Thermostat-Check.
   - Eco-Score mit Gewichten und Normierungen exakt laut Architektur 9.
   - Bremsenergie, Schub gespart, Spar-Ziel (aus, auto, fest), Spartempo.
8. **Seiten** in dieser Reihenfolge: Eco (Startseite) · Sport · Sprint · Übersicht · Großanzeige · Fahrt & Tank · Diagramme · G-Kraft (nur mit MPU6050) · Historie · Fehlercodes · Info.
9. **Sport und Sprint:**
   - Drehzahlbogen, vier Live-Balken und 30-s-Live-Diagramm mit umschaltbarem Serienpaar.
   - 0–50, 0–100 und 80–120 mit Start-, Gültigkeits- und Abbruchregeln.
   - Auto-Sprint: automatisch zur Sprintseite wechseln und wieder zurück (Architektur 10).
10. **Speichern:**
    - LittleFS A/B mit Prüfsumme, alle 60 s und bei Stillstand über 10 s.
    - Einstellungen in NVS.
    - Fahrtende ohne Uhr über GPS bzw. Kühlmitteltemperatur (Architektur 8).
    - 50 Fahrten und 100 Tankfüllungen.
    - Nie im Fahrbetrieb auf die SD-Karte schreiben.
11. **Menü** mit den Dialogen Helligkeit, Spar-Ziel, Fahrzeugart, Wartung und Diagnose, dazu die Schalter und Aktionen aus dem UI-Entwurf. Alle Overlays außer dem Tank-Fenster schließen nach 60 s ohne Berührung.
12. **Fehlercodes:**
    - Mode 03 und 07 mit deutschem Klartext aus `data/dtc_de.csv` (etwa 300 generische P0-Codes), sonst "Herstellerspezifischer Code".
    - Löschen (Mode 04) nur bei Drehzahl 0 und mit Sicherheitsabfrage.
13. **Optionale Sensoren:**
    - MPU6050: Einbaulage lernen, Steigung, Gyro-Nullpunkt im Stand.
    - GPS: Uhrzeit, Datum, km-Faktor, Tag/Nacht nach Sonnenstand.
    - Ohne den jeweiligen Sensor sind die zugehörigen Seiten und Menüpunkte ausgeblendet.

## Gestaltung

- **Farbtokens:**

  | Token | Farbe |
  |---|---|
  | `bg` | `#0E1114` |
  | `surface` | `#171B20` |
  | `line` | `#262C33` |
  | `text` | `#E8EAED` |
  | `muted` | `#8B949E` |
  | `accent` | `#7FA7C4` |
  | `good` | `#5FB98B` |
  | `warn` | `#D6A24A` |
  | `bad` | `#D46A5E` |

- Leg die Tokens in `ui/theme.*` zentral ab. Farbe trägt nur Bedeutung, sonst Grautöne. Der Stil ist schlicht, dunkel und zeitlos.
- **Farbschwellen für Verbrauch:** `good` unter 95 % des Bezugs, `warn` über 110 %. Der Bezug ist das Spar-Ziel, ohne Ziel der Tank-Schnitt. Eco-Score: ab 80 `good`, unter 60 `warn`.
- **Layout:**
  - Statusleiste 20 px: Verbindungspunkt, Seitenname, Seitenpunkte, MIL-Symbol, Tanksymbol mit Litern, Uhrzeit nur mit GPS.
  - Darunter 220 px Inhalt.
  - Übernimm Positionen und Größen aus der Vorschau. Sie ist in Display-Pixeln gebaut, ein CSS-px entspricht einem Display-Pixel.
- **Leitlinie von Jo:** Nichts überladen. Jedes Element erscheint nur dort und nur dann, wo es gebraucht wird. Hinweise zeigen sich nur im akuten Fall und verschwinden, sobald der Anlass vorbei ist.
- **Bedienung:**
  - Wischen links/rechts wechselt die Seite (Endlosschleife).
  - Tippen zeigt Details.
  - Langes Drücken (0,8 s) auf eine Kachel wählt ihren Wert.
  - Langes Drücken auf eine Fläche öffnet das Menü.
  - BOOT-Taste: kurz = nächste Seite, lang = Menü.
- **Zahlenformat:** Dezimalkomma, Tausenderpunkt ab 1000, Einheiten klein in `muted`.

## Code-Struktur

Wie in Architektur, Kapitel 12. Wichtig:
- `calc/*` ist reines C++ ohne Arduino- und LVGL-Abhängigkeit, damit es nativ testbar ist. Zeit und Messwerte kommen als Parameter herein.
- `config.h` sammelt alle Schwellen mit Kommentar und Fundstelle in der Architektur. Keine magischen Zahlen im Code.
- Jede Seite ist eine eigene Datei unter `src/ui/pages/`, mit einheitlicher Schnittstelle `create(parent)`, `update(const CarSnapshot&)` und `onShow()`.
- Die Seiten aktualisieren nur geänderte Werte. Diagramme zeichnen höchstens mit 5 Hz.

## Unit-Tests (Umgebung `native`)

Schreib für jedes Rechenmodul Tests mit diesen Prüfwerten (alle aus der Architektur):

| Test | Eingabe | Erwartung |
|---|---|---|
| Speed-Density | 32 kPa, 1,149 l, 780 U/min, VE 0,85, 25 °C, AFR 14,7, Trims 0, `fuel_cal` 1 | ≈ 0,78 l/h (± 0,02) |
| Schub | Status 0x03 = 4 | 0,0 l/h |
| l/100 km | 4 km/h | kein Wert (Anzeige l/h) |
| Ringpuffer 10 km | 12 km gefahren, die ersten 2 km mit 10 l/100 km, dann 5 l/100 km | Ø 10 km = 5,0 |
| Tank-Schnitt | 20 km seit Tanken, vorige Füllung 6,5 l/100 km | 6,5; ab 30 km der eigene Wert |
| Mischpreis | 15 l Rest zu 1,799 € + 30 l zu 1,699 € | 1,7323 €/l |
| Preis-Eingabe | "1,79" | 1,799 €/l; Vorbelegung aus 1,699 ergibt "1,69" |
| Tankmodell | Rest 28,2 l, 10 l getankt, nicht voll, Tank 49 l | 38,2 l; vollgetankt: 49 l |
| Kalibrierung | getankt 40 l, berechnet 36,4 l, 400 km | `fuel_cal` · √(40/36,4) = · 1,0483 |
| Kalibrierung ungültig | Verhältnis 1,4 oder < 150 km | keine Änderung |
| Reichweite | 28 l Rest, Ø100 6,0, Ø10 7,0, Tankfüllungen 6,2 | Prognose 6,34 → 441 km |
| Eco-Score | Rollen 20 % der Fahrzeit, Pedal 2 %/s, Schaltempfehlung 5 % offen, gebremst 0,75 l/100 km, Leerlauf 10 % | Teile 80 / 75 / 80 / 50 / 66,7 → Score 74 |
| Bremsenergie | 1150 kg, 50 → 30 km/h in 4 s, cw·A 0,70 | Bremsleistung > 0, Liter = J ÷ 8 MJ |
| Leistung | 50 km/h, 2 m/s², 1150 kg, cw·A 0,70 | ≈ 35 kW |
| Schaltempfehlung | 2300 U/min, Gang 2 (k 13), Gang 3 (k 19,5), Pedal 40 % | ja (nächster Gang 1533 U/min); bei 1800 U/min nein |
| Gang lernen | Histogramm mit Häufungen bei 7,3 / 13,2 / 19,5 / 26 / 32 | 5 Gänge erkannt, Zuordnung ± 6 % |
| Sprint | Pedal 98 %, 3800 U/min nach 1,2 s Stand | gültig; normales Anfahren mit 45 % wird verworfen; Pedal 0,6 s unter 50 % bricht nicht ab, 1,6 s bricht ab |
| Auto-Sprint-Rücksprung | Ziel erreicht | Rücksprung nach 4 s; manueller Seitenwechsel hebt ihn auf |
| Fahrtende | Abstellen bei 88 °C, Start bei 85 °C | Fahrt läuft weiter; Start bei 60 °C → neue Fahrt |
| Spar-Ziel auto | Ø der letzten 5 Füllungen 6,2 bzw. 4,6 | 5,7 bzw. 4,3; nie unter 3,0 |

## Simulator

Die Umgebung `simulator` spielt eine Fahrt nach dem Muster der Vorschau ab: Kaltstart, Stadt, Landstraße bis 100 km/h, Schub, Ampel-Leerlauf, Segeln, ein Fehlercode (P0171) und ein Tankvorgang. Dazu kommt eine Vollgas-Sequenz aus dem Stand mit Schaltpausen. Sie läuft alle paar Minuten automatisch oder nach einem langen Druck auf die BOOT-Taste im Simulator. Alle Seiten, Hinweise und Dialoge müssen damit ohne Auto prüfbar sein.

## Lieferung in Etappen

Jede Etappe kompiliert ohne Warnungen in `freenove` und `simulator`, und `pio test -e native` ist grün. Liefere am Ende jeder Etappe:
1. alle neuen und geänderten Dateien vollständig, ohne Auslassungen wie "… wie vorher"
2. eine kurze Liste, was Jo am Gerät prüfen soll
3. die getroffenen Annahmen

Hör nach jeder Etappe auf und warte auf Jos Rückmeldung.

1. **Gerüst:**
   - `platformio.ini` mit drei Umgebungen, Partitionstabelle, `board_fnk0104b.h`, `config.h` und `lv_conf.h`.
   - Display, Touch und LVGL mit Theme, Statusleiste und Wischnavigation über leere Seiten.
   - BOOT-Taste, Helligkeit per PWM.
   - Simulator-Datenquelle mit `CarState` und Snapshot.
2. **Verbindung:**
   - BLE-Link (aus `ble_serial`), ELM327-Client mit Antwort-Parser (auch für Multi-PID).
   - PID-Scheduler, unterstützte PIDs, VIN, Startbildschirm, Neuverbindung.
   - Diagnose-Dialog mit Abfragen pro Sekunde und VIN.
3. **Rechnen und Speichern:**
   - `calc/fuel`, `averages`, `trip` und Tankmodell mit allen Unit-Tests.
   - `storage` mit LittleFS A/B, NVS und Profilen (Assistent "Welches Fahrzeug?"), Fahrtende-Logik.
4. **Eco-Seite:**
   - Momentanverbrauch, Eco-Kurve, Gang mit Pfeil, Hinweis-Feld mit allen Regeln.
   - Untere Leiste mit Eco-Score, Schub und Gebremst.
   - Gang-Lernen mit Fahrzeug-Prüfung ("Fährst du mit Fahrzeug …?", Architektur 6), Start-Karte.
5. **Alltagsseiten:**
   - Übersicht mit belegbaren Kacheln und Detailverlauf, Großanzeige, Fahrt & Tank.
   - Tank-Fenster mit Ziffernfeld und automatischer Tankerkennung.
   - Diagramme (1/5/30 min, zwei Serien).
6. **Sport und Sprint:**
   - Drehzahlbogen, Live-Balken, 30-s-Puffer und Diagramm.
   - Messungen, beste Kurve, Auto-Sprint.
7. **Auswertung und Menü:**
   - Historie mit Fahrten, Tankfüllungen, Auswertung und Spartempo.
   - Fehlercodes mit Klartext-Tabelle und Löschen, Info-Seite.
   - Menü mit allen Dialogen, Wartung, Spar-Ziel, Fahrzeugart, Auto-Close nach 60 s.
8. **Optionale Sensoren:**
   - MPU6050 mit Einbaulage, Steigung und G-Kraft-Seite.
   - GPS mit Uhrzeit, Datum, km-Faktor und Sonnenstand.
   - CSV-Export auf SD, Thermostat-Check.
   - README für Jo: Flashen, erste Fahrt, Kalibrierung mit Beleg-Litern, Fehlersuche.

## Qualitätsregeln

- Kein blockierendes `delay()` in Tasks außer kurzen `vTaskDelay`. BLE-Antworten werden nicht-blockierend mit Timeout verarbeitet.
- Jeder Wert im `CarState` trägt einen Zeitstempel. Werte älter als 3 s zeigt die UI als "–" an, nie als eingefrorene Zahl.
- Division durch 0 und fehlende PIDs sind abgefangen. Fehlt eine Quelle, zeigt die Seite einen ruhigen Hinweis ("Auto liefert keinen Füllstand"), nie Unsinn.
- Speicherbedarf: Prüfe am Ende jeder Etappe RAM und Flash in der Build-Ausgabe und nenne die Zahlen.
- Keine Hersteller-PIDs, kein WLAN, keine Töne.
- Erfinde keine Funktionen, die nicht in den Unterlagen stehen. Verbesserungsvorschläge nennst du am Ende der Etappe als Frage.

Beginne mit Etappe 1.
