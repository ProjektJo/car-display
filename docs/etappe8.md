# Etappe 8: Optionale Sensoren

## Was neu ist

- **MPU6050** (optional, I2C 0x68, wird beim Start erkannt):
  - **Einbaulage lernen:** 3 s Stillstand ergeben "oben", die erste geradeaus Beschleunigung aus dem Stand ergibt "vorne". Die Lage wird je Fahrzeugprofil gespeichert. Menü → Diagnose → "Sensor neu einlernen" vergisst sie.
  - **Steigung:** Längsbeschleunigung des Sensors minus Beschleunigung aus dem OBD-Tempo (langsam), dazu die Nick-Drehrate (schnell). Der Gyro-Nullpunkt wird bei jedem Stillstand über 3 s neu gemittelt.
  - **Nutzen:**
    - Leistung und Bremsenergie rechnen die Hangkraft mit.
    - Spartempo zählt nur auf ebener Strecke (unter 1 %).
    - "Sanfter Gas geben" kommt nicht beim Überholen (hohe Längsbeschleunigung) und nicht am Berg (über 3 %).
    - Die Sport-Seite zeigt "Längs-G" mit dem gemessenen Wert.
  - **G-Kraft-Seite** (nur mit Sensor): Kreis bis 0,6 g mit Punkt, Spitzenwerte quer, beschleunigen und bremsen, dazu der aktuelle Wert.
- **GPS** (optional, UART1, RX 2 / TX 3, 9600 Baud, erkannt an gültigen NMEA-Sätzen):
  - **Uhrzeit** in der Statusleiste (MEZ/MESZ mit Sommerzeit).
  - **Datum** in Fahrten und Tankfüllungen (Historie, Export).
  - **Fahrtende:** Liegen zwischen Abstellen und Start mehr als 5 min, beginnt eine neue Fahrt.
  - **km-Faktor:** Nach je 20 km mit gutem Empfang (mindestens 5 Satelliten, über 30 km/h) wird der Faktor zur Hälfte an die GPS-Strecke angeglichen, Grenzen 0,9–1,1.
  - **Helligkeit "Auto":** Sonnenauf- und -untergang am aktuellen Ort (NOAA-Näherung) mit 15 min Übergang zwischen Tag- und Nacht-Helligkeit.
- **CSV-Export** auf microSD (Menü → Diagnose → "Fahrten exportieren", nur wenn eine Karte steckt): `cardisplay_fahrten.csv` und `cardisplay_tankfuellungen.csv`, mit Semikolon und Dezimalkomma für Excel. Geschrieben wird nur im Stand.
- **Thermostat-Check:** Dauert die Fahrt länger als 15 min, davon über 8 min schneller als 50 km/h, und bleibt das Kühlmittel trotzdem unter 75 °C (Ansaugluft beim Start über −5 °C), erscheint einmal in Bernstein "Motor bleibt kalt · Thermostat?". Danach stehen auf der Seite Fehlercodes ein umrandeter Eintrag "Hinweis · kein Fehlercode" und eine Zeile in der Start-Karte. Beides verschwindet, sobald der Motor wieder normal warm wird. Der Hinweis kommt höchstens alle 10 Starts.
- **Menü → Diagnose → "Sensoren"** zeigt, was erkannt ist: MPU6050 (lernt/bereit), GPS (Fix, Satelliten) und microSD.
- **Simulator:** Er spielt MPU6050 und GPS mit. Die Uhr startet am 6.10. um 18:45 Ortszeit in Stuttgart, kurz vor Sonnenuntergang. So sind G-Kraft-Seite, Uhrzeit und Helligkeit "Auto" am Schreibtisch prüfbar.
- **README:** erste Fahrt, Kalibrierung mit Beleg-Litern, optionale Sensoren, Werkzeuge, Fehlersuche.

## Was du prüfen sollst

Mit Board, Umgebung `simulator`:

1. In der Statusleiste steht rechts die Uhrzeit (ab 18:45).
2. Die Seite G-Kraft erscheint zwischen Diagramme und Historie. Der Punkt bewegt sich in Kurven, und die Spitzenwerte zählen mit.
3. Menü → Helligkeit → "Auto (GPS)" wählen: Nach einigen Minuten (Sonnenuntergang gegen 19:00) wird das Display über 15 min auf die Nacht-Helligkeit gedimmt.
4. Menü → Diagnose → "Sensoren" zeigt "MPU6050 bereit, GPS Fix (8 Sat.)".
5. Historie → Fahrten: Fahrten, die mit GPS gespeichert wurden, zeigen ein Datum.

Im Auto (`freenove`), wenn du die Sensoren anschließt:

6. MPU6050: einmal 3 s stehen und dann geradeaus anfahren. Danach zeigt Diagnose → Sensoren "MPU6050 bereit". Im Monitor steht `Sensoren: Einbaulage gelernt und gespeichert`.
7. GPS: Im Freien kommt nach 1–2 min die Uhrzeit. Im Monitor steht `Sensoren: GPS erkannt`.
8. microSD: Im Stand "Fahrten exportieren". Die CSV-Dateien lassen sich in Excel öffnen.

## Annahmen

- **MPU6050-Kennung:** Akzeptiert werden auch Nachbauten mit der Kennung 0x70, 0x72 oder 0x98.
- **Einbaulage** liegt in NVS je Profil (`imu<Nr>`), nicht im Profil-JSON. So muss calcTask dafür nicht das Profil umschreiben.
- **Schwellen beim Lernen:** "still" heißt Drehrate unter 1,7 °/s, "geradeaus" unter 3 °/s. "Deutlich beschleunigen" heißt über 1 m/s² laut OBD, 1,5 s lang.
- **Temperaturdrift:** Die Firmware gleicht sie über den Gyro-Nullpunkt aus, der bei jedem Halt neu gemittelt wird, statt über eine eigene Temperaturkurve.
- **G-Kraft-Punkt:** Er zeigt die Kraft, die man spürt. In einer Linkskurve wandert er nach rechts, beim Bremsen nach oben (in der Vorschau umgekehrt).
- **Fahrtende mit GPS:** Ist die GPS-Uhrzeit beim Start noch nicht da, entscheidet wie bisher die Kühlmitteltemperatur.
- **Zeitzone:** fest Deutschland (MEZ/MESZ).
- **Monate bei der Wartung** und eine Monatsansicht der Kosten mit GPS sind nicht eingebaut. Die Wartung zählt nur km.
- **SD-Karte:** Sie wird beim Start nur erkannt (eingebunden, nichts geschrieben) und beim Export beschrieben.
- **Thermostat-Hinweis:** Er erscheint auch bei abgeschalteten Spartipps, weil er selten und wichtig ist.
