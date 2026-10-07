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

## Nachtrag 2 (7.10.2026 Nachmittag)
- Verbrauch ohne Zuschnitt auf ein Auto: Quellen in der Reihenfolge Kraftstoffrate (0x5E), Luftmasse (0x10),
  **absolute Last (0x43)** (Luft je Hub vom Steuergerät, `fuel::absLoadAirGs`), erst dann Speed-Density mit
  geschätztem Füllgrad. Die jeweilige Quelle wird schnell abgefragt. Kein manueller Abgleich.
- Schub: auch ohne 0x03 = 4, wenn das Gaspedal losgelassen ist (> 1200 U/min, > 15 km/h).
- Tempo: Anzeige = OBD-Wert, mit GPS-Fix das GPS-Tempo. Strecke und l/100 rechnen mit dem per GPS gelernten km-Faktor.
- Reichweite: unter 50 km bernstein mit km in der Statusleiste und Warnfenster, unter 20 km rot blinkend und
  zweites Fenster „Sofort tanken!“. Wieder scharf nach dem Tanken.
- Tipps: bis 20 s, Fahrtipps mindestens 10 s.
- Menü: eine scrollbare Liste großer Zeilen mit Symbolen, Häufiges oben, „Selten“ darunter; Schalter AN/AUS.
- Sprint: Bestzeit fein gestrichelt, GO-Feld links unter der Zeit (verdeckt die Kurve nicht).
  Auto-Sprint springt nicht mehr zurück, solange die Messung läuft (Rohpedal lag unter 50 %).
  Abbruch bei weniger als 0,4 m/s² über 3 s. Serielle Diagnose „Sprint: …“ mit Abbruchgrund.
- Eco: Wert des Fahrt-Punkts wird so platziert, dass er keinen festen Wert oder Punkt überdeckt.

## Nachtrag 3: Fahrzeugdaten beim Einrichten (7.10.2026)
- Assistent „Neues Fahrzeug“ fragt Name, Kraftstoff, Hubraum, Tank, Leistung und Art ab und schließt nicht
  mehr von selbst (vorher entstand nach 60 s ohne Eingabe ein Profil mit Standardwerten).
- Menü → „Fahrzeug“ öffnet dieselben Felder für das geladene Profil (Befehl `SetVehicle` an calcTask,
  storageTask übernimmt Kraftstoff, Hubraum, Tank und Leistung ins Profil).
- GPS-Modul ist wahlweise: ohne GPS keine Uhr, kein Auto-Tag/Nacht, Tempo vom Auto, km-Faktor bleibt 1.

## Nachtrag 4: Tank, Reserve, Helligkeit (7.10.2026)
**Füllstand (vorhandene Logik bleibt, ergänzt):** 0x2F → Liter = Füllstand · Tankgröße (geglättet). Fehlt 0x2F oder
liefert er nichts, rechnet das Tankmodell (`tankModelL`, dauerhaft in PersistState) mit dem Verbrauch der besten
Quelle weiter (0x5E, MAF, absolute Last, zuletzt Saugrohrdruck + Drehzahl). Anzeige immer in Litern.
Diesel: Luftmasse nur zusammen mit λ (0x44), stöchiometrisch 14,5 (Benzin 14,7); ohne λ wäre der Wert beim mager
laufenden Diesel um das 1,3- bis 3-Fache zu hoch.

**Tankerkennung:**
- Mit 0x2F: Anstieg beim Start wie bisher → Tank-Fenster mit erkannten Litern, jetzt mit 15-s-Zeitlimit
  (Balken). Ohne Berührung: nicht getankt.
- Ohne 0x2F: beim Start „Getankt?“ mit drei Knöpfen (✓ Voll, ✎ Liter, ✕ Nein) und 15-s-Zeitlimit, wenn der Motor
  noch warm ist (≥ 50 °C, also kurzer Halt wie beim Tanken) und der Tank höchstens halb voll ist; ist der
  Inhalt unbekannt, bei jedem Start. Ohne Antwort: nicht getankt, das Modell rechnet mit dem gespeicherten Inhalt.
  „Voll“ bucht Tankgröße minus physischen Rest.

**Reserve:** pauschal 4 % des Tanks (1–3 l), im Fahrzeug-Setup in 0,5-l-Schritten änderbar. Angezeigt und für die
Reichweite genutzt wird nur der nutzbare Teil (physisch − Reserve, nicht unter 0). Intern bleibt der physische
Inhalt (`tankPhysL`): Vollgetankt, Mischpreis und Tankmodell rechnen damit.

**Helligkeit Auto (GPS):** Übergang über 3 h um Sonnenauf- und -untergang, in 5-%-Stufen.

**Auto-Sprint:** wechselt nur noch von der Sport-Seite zur Sprint-Seite (gemessen wird weiterhin immer).
