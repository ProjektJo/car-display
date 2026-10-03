# Zusatzfunktionen: Wo und wie (Jos Auswahl 1, 2, 9, 10, 12, 13, 20)

Stand 3. Oktober 2026. Leitlinie: **keine neue Seite, kein neues Dauer-Element auf der Eco-Seite.** Jede Funktion bekommt den Ort, an dem man sie ohnehin sucht, und zeigt sich nur dann aktiv, wenn es einen Anlass gibt.

| Nr. | Funktion | Art | Ort | Begründung |
|---|---|---|---|---|
| 2 | Verlorene Bremsenergie | fest eingebaut | Eco-Seite, untere Leiste: ersetzt "Leerlauf". Leerlauf wandert nach Fahrt & Tank | Für Hypermiling wichtiger als die Leerlaufzeit, die ohnehin einen eigenen Tipp hat. Kein zusätzliches Element |
| 13 | Spar-Ziel | fest, nur wenn gesetzt | Eco-Kurve: grüne gestrichelte Ziellinie; der erste Punkt (Tank-Schnitt) bleibt; Punktfarben beziehen sich dann aufs Ziel. Fahrt & Tank: Zeile "Ø / Ziel" | Eine Bezugslinie statt zwei, die Kurve bleibt ruhig. Einstellen im Menü: Aus, Auto (0,5 l unter dem Schnitt der letzten Tankfüllungen, unter 5 l Schnitt 0,3 l) oder fest 3,0–7,0 |
| 1 | Sparsamste Geschwindigkeit | optionale Einsicht | Historie, vierter Chip "Spartempo": Kurve l/100 km je Tempo, Minimum markiert | Ist eine Auswertung, braucht man nicht beim Fahren. Lernt nur aus ebener Konstantfahrt im höchsten Gang |
| 12 | Zusammenfassung letzte Fahrt | Popup beim Start | Karte über der Eco-Seite nach dem Verbinden, 6 s oder bis Tippen oder Losfahren | Einziger Moment, in dem man Zeit hat. Verschwindet von selbst |
| 9 | Wartungserinnerung | nur akut + Menü | Eine Zeile in der Start-Karte, wenn fällig in < 500 km (bernstein) oder überfällig. Stand und Einstellung im Menü "Wartung" | Sonst unsichtbar. Nach "Erledigt" im Menü startet der Zähler neu |
| 10 | Thermostat-Check | nur akut | Einmaliger Hinweis im Hinweis-Feld links neben dem Gang in `warn`: "Motor bleibt kalt · Thermostat?" Danach als Eintrag "Hinweis (kein Fehlercode)" auf der Seite Fehlercodes | Selten, aber teuer. Höchstens einmal pro Tag (ohne Uhr: alle 10 Starts), nur wenn eindeutig |
| 20 | Helligkeit nach Sonnenstand | unsichtbar | Menü "Tag/Nacht: Auto (GPS)", nur angeboten wenn GPS steckt | Reine Automatik, braucht keine Anzeige |

## Logik

**Bremsenergie (2)**
- Bremsleistung = max(0, −m·a − Luftwiderstand − Rollwiderstand) · v. Was über das natürliche Ausrollen hinaus verzögert, ist Bremsen. Mit MPU6050 inklusive Steigung (bergab ist Ausrollen stärker).
- Liter = Energie ÷ (Motorwirkungsgrad 0,25 · Heizwert; Benzin 32 MJ/l, Diesel 36 MJ/l). Anzeige je Fahrt in Litern, gespeichert im Fahrtdatensatz, in der Historie als Spalte.

**Spar-Ziel (13)**
- Gilt für die laufende Tankfüllung. Fahrt & Tank zeigt "Ø 5,8 / Ziel 5,5" in `good` oder `warn`. Historie → Tankfüllungen zeigt das Ziel als Linie.

**Spartempo (1)**
- Tempo-Klassen 30 … 130 km/h in 10er Schritten. Gezählt wird nur, wenn: höchster gelernter Gang, Tempo ± 3 km/h über 20 s, Steigung |< 1 %| (mit MPU) bzw. Gaspedal ruhig (ohne MPU). Je Klasse Liter und km aufsummieren.
- Eine Klasse zählt erst ab 5 km Daten; vorher steht "noch zu wenig Daten".

**Start-Karte (12)**
- Inhalt: Strecke, Ø-Verbrauch, Kosten, Eco-Score, Gebremst der letzten Fahrt; darunter höchstens eine Hinweiszeile (Wartung oder Thermostat).
- Erscheint nicht, wenn die letzte Fahrt kürzer als 1 km war.

**Wartung (9)**
- Menü: Ölwechsel alle X km (Standard 15 000) und Inspektion alle Y km, jeweils "Erledigt" setzt den Stand auf den aktuellen Gesamt-km. Mit GPS zusätzlich nach Monaten.
- Die Gesamt-km zählt die Firmware selbst (OBD-Tempo). Den aktuellen Tachostand trägt man beim Einrichten einmal ein.

**Thermostat (10)**
- Bedingung: Fahrzeit > 15 min, davon > 8 min über 50 km/h, Kühlmittel < 75 °C, Außenluft (Ansaugluft beim Start) > −5 °C. Bei Frost deutlich gelockert oder aus, weil dann auch ein gesundes Auto langsam warm wird.

**Sonnenstand (20)**
- Sonnenauf- und -untergang aus GPS-Position und Datum (Standardformel, kein Internet nötig). 15 min Übergang.

## Bewusst weggelassen
- Keine eigene Seite für Wartung oder Spartempo.
- Bremsenergie nicht als Live-Zahl, sondern als Summe je Fahrt. Live würde sie bei jedem Bremsen zappeln und ablenken.
