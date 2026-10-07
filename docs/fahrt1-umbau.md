# Umbau nach der ersten Fahrt (7.10.2026)

Jos Rückmeldung nach der ersten Fahrt und was daraus wurde.

## 1. Eco-Kurve: Fahrt-Punkt
- Die gestrichelte Fahrt-Linie steht ab 0,5 km immer an der km-Stelle der Fahrt (logarithmisch zwischen
  1 / 10 / 100 km, darüber Richtung „Tank“ nach den km seit dem Tanken, `eco::tripAxisPos`).
- Der Punkt mit dem Ø der Fahrt kommt erst ab 10 km dazu (`TRIP_DOT_MIN_KM`) und ist dann Teil der runden Kurve.
- Die Fläche unter der Kurve hat einen Verlauf: oben kräftig, unten durchsichtig. Der Verlauf hängt an der
  absoluten Höhe, damit niedrige und hohe Stellen gleich wirken.

## 2. Größere Zahlen
- Neue Schriften: `font_v24`, `font_v32`, `font_v40` (LVGL-Montserrat, Umlaute aus den eigenen Schriften).
- Eco: Momentanverbrauch 48 px ohne Überschrift, Gang 40 px. Solange ein Hinweis steht, entfällt die Einheit.
- Detail-Fenster (Tippen auf Kachel oder Feld): Wert 40 px.

## 3. Untere Felder frei belegbar
- `FieldBar` (src/ui/field_bar.*): Beschriftung oben, Wert mittig groß. Passt ein Wert nicht, wird die Schrift
  kleiner und dann die Einheit weggelassen. Tippen = Detail, lang drücken = Wert wählen.
- Belegung in `UiSettings::fields` (Eco, Sport; Sprint reserviert). `UiSettings` wird nur hinten erweitert,
  `store::loadUi` übernimmt kürzere Einträge alter Firmware (Drehung, Helligkeit bleiben erhalten).
- Neue Werte: Eco-Score, Schub gespart, Gebremst, Ø Fahrt, Strecke, Fahrzeit, Ø Tempo, Vmax, Spitze, Leistung,
  Tankinhalt, Kosten Fahrt. Die Auswahl ist scrollbar.

## 4. Sprint
**Warum nichts gemessen wurde:** Der Start verlangte Gaspedal ≥ 85 % (Rohwert) und ≥ 3000 U/min. Viele Autos melden
bei Vollgas nur 70–80 % Pedal, und beim Anfahren werden 3000 U/min oft nicht erreicht.

Neu:
- Pedal relativ zum gelernten Bereich (leer … Vollgas; bis ein höherer Wert gesehen wurde, gilt „leer + 55 %“
  als Vollgas). Sprint ab 80 % **oder** im Schnitt ≥ 9 km/h je s (2,5 m/s²) ab 1 s nach dem Anfahren.
  Keine Drehzahl-Bedingung mehr.
- Abbruch nur noch, wenn das Tempo um > 3 km/h fällt oder die Messung länger als 30 s dauert.
- 80–120 startet mit Pedal ≥ 80 % oder ab 1,5 m/s² bei 80 km/h.
- Jedes Anfahren aus dem Stand zeigt die Live-Kurve („LIVE“, zählt nicht); nur ein erkannter Sprint („GO“) zählt.
- Seite: großes Diagramm, „READY“ grün im Stand, „GO“ klein orange beim Sprint, Ergebnis grün. 0–50 / 0–100 /
  80–120 als Chips unten, Tippen zeigt letzte und beste Zeit groß mit Abstand.

## 5. Sport
- Großer Drehzahlbogen mit Tempo 48 px, Gang, rechts die vier Balken (größer beschriftet).
- Wettbewerbszeile: nach einem Sprint-Ergebnis Zeit und Abstand zur Bestzeit; beim Beschleunigen Prozent der
  besten Beschleunigung dieser Sitzung (grüne Marke am Balken); sonst der Schnitt-Trend der letzten 2 min.
- Unten vier frei belegbare Felder (Ø Tempo, Strecke, Fahrzeit, Leistung).
- Live-Diagramm: Tippen auf den Bogen öffnet es groß, Paar umschaltbar.

## Nachtrag (Jos Antworten, 7.10.2026)
- Sprint: Ab 2 m/s² (7,2 km/h je s) zählt das Anfahren immer. Abbruch, wenn die Beschleunigung im Verlauf
  über 3 s unter 0,8 m/s² fällt (`SPRINT_MIN_ACCEL_MS2`, `SPRINT_ACCEL_WINDOW_MS`).
- Sport: statt des km/h-Trends „Zeit gewonnen“: Die Strecke der letzten 2 min, mit dem Fahrtschnitt von
  davor gefahren, hätte so viel länger gedauert („12 s gewonnen · Ø +3,0 km/h“).
- Eco: Momentanverbrauch wieder 38 px mit Überschrift (gut lesbar), Felder 24 px.
