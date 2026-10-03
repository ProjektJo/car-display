# UI-Entwurf: Car-Display v2

Stand 3. Oktober 2026 nach der Gesamtprüfung (mit Zusatzfunktionen, siehe `zusatzfunktionen.md`), Schritt 3 des Meilensteins. Querformat 320 × 240 px, LVGL 9. Die klickbare Vorschau zeigt jede Seite in Originalproportion mit einer simulierten Fahrt.

## Gestaltung

**Stil:** dunkel, ruhig, große Zahlen. Farbe trägt nur Bedeutung (sparsam / normal / Achtung), sonst Grautöne und ein gedämpftes Blau als Akzent.

| Token | Farbe | Verwendung |
|---|---|---|
| `bg` | `#0E1114` | Hintergrund |
| `surface` | `#171B20` | Kacheln, Dialoge |
| `line` | `#262C33` | Trennlinien, Diagrammraster |
| `text` | `#E8EAED` | Werte |
| `muted` | `#8B949E` | Beschriftungen, Einheiten |
| `accent` | `#7FA7C4` | aktive Elemente, Diagrammlinie |
| `good` | `#5FB98B` | sparsam, unter Durchschnitt, Schub |
| `warn` | `#D6A24A` | kalter Motor, über Durchschnitt, Reichweite < 50 km |
| `bad` | `#D46A5E` | nur Fehlercodes und Löschen-Dialog |

**Nachtmodus:** gleiche Farben, Hintergrundbeleuchtung nach Einstellung (Standard 25 %), `text` auf `#C9CDD2` abgedunkelt.

**Schrift:** Montserrat (in LVGL enthalten). Größen: 12 (Beschriftung), 14 (Text), 20 (Kachelwert), 28 (Seitenwert), 48 und 72 (Großanzeige, nur Ziffern und `,.-/` als eigener Zeichensatz, spart Flash). Zahlen mit Komma, eine Nachkommastelle beim Verbrauch.

## Rahmen jeder Seite

```
┌──────────────────────────────────────────┐
│ ● Eco         ○○●○○○○  ⚠ MIL  ⛽ 28 l  14:32 │ 20 px Statusleiste
├──────────────────────────────────────────┤
│                                          │
│              Seiteninhalt                │ 220 px
│                                          │
└──────────────────────────────────────────┘
```
- **Links:** Verbindungspunkt (grün verbunden, blinkend grau "verbinde", grau getrennt) und Seitenname.
- **Mitte:** Seitenpunkte.
- **Rechts:** Motor-Symbol, wenn die MIL an ist; dezent Tanksymbol mit Litern im Tank (`muted`, bei Reichweite < 50 km `warn`); Uhrzeit nur mit GPS.

## Bedienung

| Geste | Wirkung |
|---|---|
| Wischen links/rechts | Seite wechseln (Endlos-Schleife) |
| Tippen auf Wert/Kachel | Detail: Verlauf dieses Werts (5 min), Min/Max, Zurück mit Tippen |
| Lang drücken auf Kachel (0,8 s) | Wert für diese Kachel auswählen (Liste), wird gespeichert |
| Lang drücken auf leere Fläche oder Statusleiste | Menü |
| Wischen von oben nach unten | Menü (Alternative) |
| BOOT-Taste kurz / lang | nächste Seite / Menü |

Menü, Einstellungs-Dialoge, Kachelauswahl, Detailansicht und Löschen-Abfrage schließen nach 60 s ohne Berührung, das Display steht dann wieder auf der Seite von vorher. Ausnahme: das Tank-Fenster bleibt, bis es beantwortet ist.

## Seiten

Reihenfolge = Wischreihenfolge, sortiert nach Nutzen beim Fahren: erst was man beim Fahren braucht, dann Auswertung, zuletzt Diagnose und Hilfe. Startseite ist Eco. Seiten ohne Daten (z. B. G-Kraft ohne Sensor) sind ausgeblendet.

1. Eco · 2. Sport · 3. Sprint · 4. Übersicht · 5. Großanzeige · 6. Fahrt & Tank · 7. Diagramme · 8. G-Kraft (nur mit Sensor) · 9. Historie · 10. Fehlercodes · 11. Info

### 1. Eco (Startseite, enthält den Verbrauch)
Eco und Verbrauch sind eine Seite (Jos Wunsch vom 2. Oktober).
```
│ Momentanverbrauch   ┌──────────┐   Gang  │
│ 13,8 l/100 km       │56° Motor │   ▲ 3   │  38 px Zahl, Farbe good/neutral/warn
│                     │ kalt …   │         │  Hinweis-Feld nur im akuten Fall, ▲ = hochschalten
│                     └──────────┘         │
│ 12┤                              ●13,8   │
│  8┤ 6,4───6,2────6,5_____╱               │  runde Kurve (Catmull-Rom) durch 5 Punkte
│  4┤ ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄ Ziel 5,5     │  gestrichelt: Spar-Ziel (grün), ohne Ziel Tank-Schnitt
│  Tank  100 km 10 km  1 km  Momentan      │
│ Score 62   Schub 0,01 l   Gebremst 0,08 l│
```
- Kurve: Y = l/100 km (0–12, Werte darüber oben mit ↑), X = Tank (seit letztem Tanken), 100 km, 10 km, 1 km (letzter Kilometer), Momentan. Die Linie wird rund interpoliert (Catmull-Rom → Bezier, in LVGL als Canvas oder `lv_chart` mit vielen Zwischenpunkten), darunter eine schwache Flächenfüllung.
- Punktfarbe: Tank `accent` (ohne Ziel), die anderen `good` unter 95 % vom Bezug, `warn` über 110 %, sonst `text`. Bezug ist das Spar-Ziel, wenn gesetzt, sonst der Tank-Schnitt. Dieselben Schwellen färben den Momentanverbrauch oben.
- Untere Leiste: Eco-Score, Schub gespart, **Gebremst** (durch Bremsen verlorene Energie in Litern, Summe der Fahrt).
- Momentan bei Stillstand: kein Punkt, "Stand", Kurve endet bei 1 km. Bei Schub: Punkt auf 0, oben "SCHUB 0,0" in `good`.
- Gang rechts oben.
- **Hinweis-Feld** links neben dem Gang (118 × 40 px, Rand und Symbol in `accent`, bei kaltem Motor und Thermostat in `warn`): Symbol + max. vier Wörter auf zwei Zeilen. Nur sichtbar, solange ein akuter Anlass besteht, höchstens 8 s. Der Stand-Hinweis wiederholt sich alle 30 s mit der Standzeit. Bei kaltem Motor: Thermometer-Symbol, daneben "Motor 57 °C" und darunter kleiner "max. 2.500 U/min", solange es gilt. "Gang rein · Schub 0 l" kommt nur, wenn ausgekuppelt gebremst wird; Segeln, das das Tempo hält, ist in Ordnung.
- **Schalthinweis** nur als grüner Pfeil ▲ (20 px) vor der Gangzahl, weg sobald hochgeschaltet ist. Während Schub bleibt das Feld leer. Regeln und Sperrzeiten: Architektur, Kapitel 9.
- Unter der Kurve steht nichts mehr; die Kurve ist dadurch etwas höher. Die Beschriftung "Ziel 5,7" steht links unter der Ziellinie.

### 2. Sport (live)
```
│   ╭─ 3 ─ 4 ─╮     Leistung     14 kW · 19 PS │
│  2    28     5    ▬▬▬───────────────────────  │
│  1   km/h    6    Beschleunigung   +1,4 m/s²  │
│  0  2 Gang  ▓     ─────────|▬▬▬▬────────────  │  Balken um die Mitte
│                   Gaspedal 47 % · Saugrohr 80 kPa (je mit Balken)
│ 0–100 läuft · 3,8 s          [— Tempo ┄ Leistung ›] │
│ 140┤ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ├60 │  Live 30 s
│   0┤ ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄ ├0  │
```
- Drehzahlbogen 240°, Strich 9 px, `accent`, ab 5800 U/min `warn`; Tempo 32 px in der Mitte, darunter Gang mit ▲.
- Rechts vier Balken (Beschriftung `muted` 10 px, Wert 11,5 px fett, Balken 5 px).
- Unten Live-Diagramm 30 s; der Chip rechts schaltet das Serienpaar (Tempo + Leistung, Drehzahl + Gas, Beschleunigung + Leistung). Erste Serie `accent`, zweite gestrichelt `muted` auf der rechten Achse.

### 3. Sprint
- Links Karte "0–100 km/h · bereit/läuft/geschafft" mit Zeit 36 px und Fortschrittsbalken, darunter "Startet aus dem Stand" bzw. das aktuelle Tempo.
- Rechts Tempoverlauf 0–100 der letzten Messung (`accent`) und der besten (`good`, gestrichelt).
- Mitte Tabelle 0–50 / 0–100 / 80–120 mit letzter und bester Zeit (beste in `good`).
- Unten vier Kacheln: Ø Fahrt, Zeit/100 km, Vmax, Spitze kW.
- Auto-Sprint: Vollgas aus dem Stand (Gaspedal ab 85 %, ab 3000 U/min) holt diese Seite von selbst nach vorn. Nach dem Ziel bleibt das Ergebnis 4 s stehen, dann kehrt die Anzeige zur vorigen Seite zurück; ebenso beim Abbruch (Gas länger als 2 s unter 50 %) oder nach 25 s. Abschaltbar im Menü.
- Gemessen wird nur ein erkannter Sprint; normales Anfahren landet nie in der Tabelle. Schaltpausen brechen die Messung nicht ab (Regeln: Architektur, Kapitel 10).

### 4. Übersicht (6 Kacheln, frei belegbar)
Standard: Tempo · Drehzahl · Momentan · Ø 10 km · Gaspedal · Reichweite. 3 × 2 Raster, je Kachel Beschriftung oben links, Wert 28 px, Einheit klein. **Reichweite überall gleich** (Kachel, Großanzeige, Fahrt & Tank): große Zahl = Reichweite, darunter ein geteilter Balken (gefahren seit Tanken `muted` | Rest `accent`, bei < 50 km `warn`) und die Zeile "⛽ ··· 316" links (km seit Tanken) und "Σ 763 km" rechts (voraussichtliche Strecke der ganzen Tankfüllung = gefahren + Reichweite).

### 5. Großanzeige
Ein Wert in 72 px, Beschriftung darüber, links/rechts kleine Pfeile zum Durchschalten der Werte. Gedacht für Mitfahrer und schnellen Blick.

### 6. Fahrt & Tank
Zwei Spalten: **Fahrt** (Strecke, Dauer, Ø erst ab 0,5 km, Kosten, Leerlauf in min, Eco-Score eingefärbt) und **Tankfüllung** (gefahren, verbraucht, im Tank, Reichweite, Ø-Preis im Tank (Mischpreis), bei gesetztem Ziel "Ø / Ziel" in `good` oder `warn`). Knopf **"Getankt"** öffnet den Tank-Dialog (Preis pro Liter eingeben, berechnete Liter bestätigen oder korrigieren). Tippen auf die Spalte öffnet das Fahrtenbuch bzw. die Liste der Tankfüllungen.

### 7. Diagramme
Linienverlauf mit Zeitwahl **1 / 5 / 30 min** oben rechts. Achsen mit runden Werten (unten, Mitte, oben, z. B. 0 / 2000 / 4000 U/min); Tempo, Drehzahl, Gaspedal und Verbrauch beginnen immer bei 0. Wertewahl durch Tippen auf die Legende: Verbrauch, Tempo, Drehzahl, Gaspedal, Kühlmittel, Spannung. Höchstens zwei Linien gleichzeitig (zweite gestrichelt, rechte Achse).

### 8. G-Kraft (nur mit MPU6050)
Kreis mit Punkt für die aktuelle Querbeschleunigung/Verzögerung, Spitzenwerte, sanftes Bremsen fließt in den Eco-Score.

### 9. Historie
Vier Ansichten, oben per Chip umschaltbar:
- **Fahrten:** Balken mit Ø l/100 km der letzten 20 Fahrten, gestrichelt der Gesamtschnitt. Balken unter 95 % des Schnitts `good`, über 110 % `warn`, sonst `muted`. Tippen auf einen Balken zeigt darunter Fahrtnummer, Strecke, Ø, Dauer, Kosten, Gebremst und Eco-Score (eingefärbt nach Bewertung).
- **Tankfüllungen:** runde Kurve Ø l/100 km je Tankfüllung (linke Achse, `accent`) und gestrichelt € pro 100 km (rechte Achse). Darunter letzte Füllung (Liter, €), Ø der letzten, Veränderung seit der ersten gespeicherten Füllung in %.
- **Auswertung:** Ø-Verbrauch nach Streckenlänge (< 5, 5–20, 20–50, > 50 km) mit Satz wie "Kurzstrecken unter 5 km brauchen 25 % mehr". Rechts Summen der letzten 50 Fahrten (km, Liter, Kosten, Ø), Eco-Score-Trend (vorige 10 → letzte 10 Fahrten), beste Fahrt über 5 km, Schubanteil.
- **Spartempo:** runde Kurve l/100 km je Tempo (30–130 km/h), sparsamster Punkt groß in `good`, darunter "Am sparsamsten bei 60 km/h · 3,8 l". Klassen mit < 5 km zeigen "?". Nur ebene Konstantfahrt im höchsten Gang.
- Ohne GPS kein Datum: Fahrten sind fortlaufend nummeriert. Mit GPS zusätzlich Datum und eine Monatsansicht der Kosten.

### 10. Fehlercodes
Liste: Code, Klartext, "gespeichert/vorläufig". Knopf "Neu lesen". Knopf "Löschen" (nur bei stehendem Motor aktiv) mit Bestätigungsdialog in `bad`. Darunter ggf. ein umrandeter Eintrag "Hinweis · kein Fehlercode" in `warn` (Thermostat-Check), den "Löschen" nicht entfernt; er verschwindet, wenn der Motor wieder normal warm wird.

### 11. Info (Kurzanleitung)
Vier Chips: **Bedienung** (Gesten, BOOT-Taste, Tank-Fenster), **Farben** (was grün, weiß, bernstein, blau, rot bedeuten), **Werte** (Tank, 100/10/1 km, Momentan, SCHUB, Reichweite, Gebremst in je einer Zeile) und **Eco-Score** (die fünf Teile mit Gewicht und je einem Satz, darunter die Bewertung ab 80 gut, 60–79 ok, unter 60 Luft nach oben). Unten Version, Fahrzeugprofil, Adapter und Abfragen pro Sekunde.

## Tank-Fenster (über jeder Seite)
Erscheint automatisch, sobald ein Tankvorgang erkannt wurde, egal auf welcher Seite (siehe Architektur 7, Tankerkennung). Von Hand über "Getankt" auf der Seite Fahrt & Tank oder im Menü.
```
│ Getankt                 Liter geschätzt über Tankanzeige │
│ Liter                │ Preis pro Liter                    │
│ [▲][▲]  [▲]          │ [▲]  [▲][▲]                        │  je Stelle ein ▲ darüber
│  2  8 ,  4  💧       │  1 ,  7  9 ⁹ €                     │  30 px Ziffern, ⁹ klein hochgestellt, Tropfen und € in accent
│ [▼][▼]  [▼]          │ [▼]  [▼][▼]                        │  je Stelle ein ▼ darunter
│ [✓ vollgetankt]                       Kosten 50,81 €    │
│ [Nicht getankt]                  [        OK        ]     │
```
- Jede Stelle hat ihr eigenes ▲ und ▼ (Tasten 32 × 30 px). Überlauf rechnet weiter (1,79⁹ + 10 ct = 1,89⁹). Eingegeben werden nur Euro und Cent (auch im Ziffernfeld höchstens zwei Nachkommastellen). Die 9/10 Cent der Zapfsäule stehen fest als kleine hochgestellte ⁹ dahinter und werden immer mitgerechnet: Eingabe 1,79 ergibt 1,799 €/l. Hinter den Litern ein Tropfen-Symbol, hinter dem Preis ein €. Damit braucht es keine Beschriftungen wie "+10 ct". Führende Null bei Litern unter 10 grau.
- Vorausgefüllt mit dem letzten Zapfsäulenpreis. Kein Schieberegler, der wäre auf dem kleinen Touch zu ungenau.
- Tippen auf den Preis öffnet ein Ziffernfeld.
- "Nicht getankt" verwirft eine Fehlerkennung.
- Schalter **✓ vollgetankt** (vorgewählt, `good`) bzw. "nicht voll" (`muted`): Bei Teilbetankung antippen. Ohne Tankanzeige ist der Tank danach voll bzw. Rest + Liter; kalibriert wird nur zwischen zwei Vollbetankungen (Architektur 7).
- Bei manueller Eingabe ohne Tankanzeige stehen dort die berechneten Liter mit "berechnet seit der letzten Füllung".

## Menü (Overlay)
Zweispaltig, jede Zeile zeigt rechts den aktuellen Wert. Unterdialoge haben oben rechts "Fertig" (zurück zum Menü).
- **Helligkeit:** Chips Tag / Nacht / Auto (GPS), darunter je eine Zeile Tag und Nacht mit großen − / + Tasten (40 px).
- **Spar-Ziel:** Chips Aus / Auto / Fest, darunter −0,5 · − · Wert · + · +0,5. Bei Auto steht darunter, woraus der Wert kommt ("6,2 → 5,7"). − oder + bei Auto macht daraus ein festes Ziel.
- **Fahrzeugart:** Liste Kleinwagen, Kompakt, Limousine, Kombi, SUV, Van, Transporter mit Gewicht; die gewählte in `accent`.
- **Wartung:** Ölwechsel und Inspektion je mit "fällig in … km · alle … km" (unter 500 km in `warn`) und Knopf "Erledigt".
- **Diagnose:** Adapter, Protokoll, Abfragen/s, unterstützte PIDs, Verbrauchsquelle mit Kalibrierfaktor, Fahrzeugart.
- Weitere Zeilen, jede tippbar: Getankt von Hand · Kacheln zurücksetzen · Spartipps an/aus · Kalt-Grenze (2000–3000 U/min in 250er Schritten) · Auto-Sprint an/aus · Fahrt beenden (speichert und beginnt eine neue) · Info (springt zur Info-Seite). Den Spritpreis gibt man im Tank-Fenster ein.
- Zahlen ab 1000 mit Tausenderpunkt (15.000 km).
- Seltenes im Diagnose-Dialog: Fahrzeugprofil wechseln/neu (Assistent), Kühlmittelgrenze, Mittelwerte zurücksetzen (je Fenster), Sensor neu einlernen (nur mit MPU6050), Fahrten exportieren (nur mit SD).

## Start-Karte (nach dem Verbinden)
Karte über der Eco-Seite: "Letzte Fahrt" mit Fahrtnummer, Strecke und Dauer, darunter Ø, Kosten, Eco-Score, Gebremst. Die Werte kommen aus demselben Fahrtdatensatz wie der letzte Balken in der Historie. Höchstens eine Hinweiszeile in `warn` (Wartung fällig in < 500 km oder Thermostat). Schließt nach 6 s, beim Losfahren oder durch Tippen. Entfällt, wenn die letzte Fahrt unter 1 km lag.

## Startbildschirm
Verbindungsstatus statt Logo, in Schritten: "Suche Adapter …" → "Verbunden mit vLinker MC" → "Protokoll: ISO 14230" → "Fahrzeug: Renault Modus". Bei Fehlern ein Satz mit der Lösung (z. B. "Zündung an? Handy-App des Adapters schließen.").
