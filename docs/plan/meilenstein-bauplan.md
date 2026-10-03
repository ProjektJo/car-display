# Meilenstein: Firmware-Bauplan mit UI

**Ziel:** Am Ende steht ein umfassender Prompt, mit dem Claude Opus 5.5 die komplette Firmware in einem Zug baut.
**Stand:** 2. Oktober 2026 · Schritte 1–3 erledigt (Antworten in architektur.md), Abnahme durch Jo offen

## Ablauf

| Schritt | Inhalt | Ergebnis |
|---|---|---|
| 1. Anforderungen | Jo beantwortet den Fragenkatalog unten. Unbeantwortete Fragen bekommen die Empfehlung. | Festgelegte Funktionsliste |
| 2. Architektur | Module, Tasks (BLE/OBD auf Core 0, UI auf Core 1), Datenmodell, PID-Liste mit Abfragetakt, Speicher (PSRAM, Flash/SD), Fehler- und Verbindungszustände | `architektur.md` |
| 3. UI-Entwurf | Jede Seite als Skizze in Originalgröße 240x320, Farben, Schriften, Touch-Zonen, Gesten, Warn-Overlays. Klickbare Vorschau im Browser | `ui-entwurf.md` + Vorschau-Seite |
| 4. Abnahme | Jo prüft Architektur und UI, wir passen an | freigegebener Bauplan |
| 5. Master-Prompt | Alles in einen Prompt für Opus 5.5: Rolle, Hardware mit Pins, Bibliotheken mit Versionen, Architektur, jede Seite, Akzeptanzkriterien, Simulator, Teststrategie, Ausgabeformat | `master-prompt.md` |

Die bestehende Firmware unter `car-display/` (BLE, ELMduino, 5 Seiten, Simulator) dient als geprüfte Basis für Pins und BLE-Verbindung.

---

## Fragenkatalog

Antwort am einfachsten so: `1b, 2a, 3 Golf 7 1.4 TSI 2016, ...` oder "Rest wie empfohlen". **⭐ = Empfehlung**, die gilt, solange du nichts anderes sagst.

### A. Auto und Adapter

1. **Welches Auto?** Marke, Modell, Motor, Baujahr, Benzin/Diesel/Hybrid. (Bestimmt, welche PIDs es gibt und ob herstellerspezifische Werte wie Öltemperatur, Turbo-Ladedruck oder DPF-Beladung möglich sind.)
2. **Herstellerspezifische Werte (Mode 22)?** Damit gehen z. B. Öltemperatur, Ladedruck, Getriebetemperatur, DPF-Status, die Standard-OBD oft nicht liefert.
   a) ⭐ Ja, als Erweiterung vorsehen (Tabelle mit eigenen PIDs in einer Datei, erst Standard-PIDs, Mode 22 später nachrüstbar)
   b) Nur Standard-OBD2
3. **Adapter schon gekauft?** a) ⭐ vLinker MC+ BLE b) OBDLink CX c) anderer: welcher?

### B. Einbau und Strom

4. **Stromversorgung:**
   a) ⭐ USB im Auto, geht mit Zündung aus (einfach, keine Batterieentladung)
   b) Dauerplus, Display schläft selbst ein (braucht Schlafmodus und Spannungswächter)
5. **Einbauort und Licht:** Soll sich die Helligkeit anpassen?
   a) ⭐ Tag/Nacht-Modus automatisch nach Uhrzeit bzw. per Touch umschaltbar
   b) Feste Helligkeit
   c) Lichtsensor nachrüsten (Zusatz-Hardware)
6. **Ausrichtung:** a) ⭐ Querformat 320x240 b) Hochformat 240x320

### C. Was angezeigt werden soll

7. **Wichtigste Werte** (bitte die Top 5 nennen, Rest kommt auf Unterseiten): Tempo, Drehzahl, Kühlmittel, Öltemperatur, Bordspannung, Ansaugluft, Motorlast, Gaspedal, Momentanverbrauch, Ø-Verbrauch, Tankinhalt, Ladedruck, Zündwinkel, Gemischkorrektur, Außentemperatur, Restreichweite.
   ⭐ Kühlmittel, Öltemperatur (falls verfügbar), Bordspannung, Momentanverbrauch, Ladedruck/Last
8. **Seiten.** Welche willst du? (mehrere möglich)
   a) ⭐ Übersicht mit Kacheln
   b) ⭐ Großanzeige: ein Wert riesig, per Wischen wählbar
   c) ⭐ Diagramme (Verlauf)
   d) ⭐ Fahrt- und Verbrauchsstatistik
   e) ⭐ Fehlercodes
   f) Rundinstrumente im Tacho-Look
   g) Leistung/Beschleunigung (0–100, 80–120)
   h) "Motor warm?"-Ampel: wann darf ich Gas geben
9. **Diagramme:** Zeitfenster?
   a) ⭐ umschaltbar 1 / 5 / 30 min
   b) fest 5 min
10. **Darf man die Kacheln selbst belegen** (am Display antippen und Wert wählen)?
    a) ⭐ Ja, Belegung wird gespeichert
    b) Nein, fest im Code

### D. Bedienung

11. **Bedienkonzept:**
    a) ⭐ Wischen links/rechts = Seite, Tippen auf Wert = Details, langes Drücken = Menü
    b) Wie jetzt: Tippen = nächste Seite
12. **Einstellungsmenü am Gerät** (Helligkeit, Warnschwellen, Einheiten, Adapter wählen)?
    a) ⭐ Ja
    b) Nein, nur über `config.h`

### E. Warnungen

13. **Wovor soll gewarnt werden?** (mehrere möglich)
    a) ⭐ Kühlmittel zu heiß
    b) ⭐ Bordspannung zu niedrig/hoch (Lichtmaschine, Batterie)
    c) ⭐ Neuer Fehlercode / Motorkontrollleuchte
    d) ⭐ "Kalter Motor, nicht hochdrehen" (Drehzahl über Grenze bei kaltem Öl)
    e) Schaltblitz bei Wunschdrehzahl
    f) Tempo-Grenze
14. **Wie warnen?** a) ⭐ Vollbild-Banner rot blinkend, mit Tippen quittieren b) nur Farbe der Kachel c) zusätzlich Piepser (Zusatz-Hardware)

### F. Fehlercodes

15. **Fehlercodes (DTC):**
    a) ⭐ Lesen mit Klartext (eingebaute Liste der häufigsten P-Codes), Löschen nur nach Sicherheitsabfrage und bei stehendem Motor
    b) Nur lesen
    c) Gar nicht
16. **Freeze-Frame und Readiness-Monitore** (für TÜV/AU interessant) anzeigen? a) ⭐ Ja b) Nein

### G. Daten speichern

17. **Fahrten speichern?**
    a) ⭐ Ja: Fahrtenbuch mit Strecke, Dauer, Verbrauch, Maxwerte im Flash, die letzten 50 Fahrten
    b) Zusätzlich jede Sekunde alle Werte als CSV auf SD-Karte (das Freenove hat einen SD-Slot)
    c) Nein, nur aktuelle Fahrt
18. **Tankinhalt/Kosten:** Spritpreis eingeben und Kosten pro Fahrt anzeigen? a) ⭐ Ja b) Nein

### H. Verbindung nach außen

19. **WLAN:**
    a) ⭐ Nur für Updates (OTA) und zum Herunterladen der Fahrten per Handy-Browser, startet nur auf Wunsch
    b) Live-Dashboard im Handy-Browser zusätzlich
    c) Kein WLAN
20. **Uhrzeit:** Die Uhr braucht eine Quelle. a) ⭐ per WLAN/Handy beim Einrichten stellen, intern weiterzählen b) keine Uhrzeit anzeigen

### I. Aussehen

21. **Stil:**
    a) ⭐ Dunkel, modern, große Zahlen, wenige Farben (Ampelfarben nur für Zustände)
    b) Retro (Pixel/Segment-Look)
    c) Bunte Rundinstrumente
22. **Sprache und Einheiten:** a) ⭐ Deutsch, metrisch b) Englisch
23. **Startbildschirm:** a) ⭐ Eigenes Logo/Name (welcher?) b) einfach Verbindungsstatus

### J. Umsetzung

24. **Bestehende Firmware:**
    a) ⭐ Als Basis nehmen und gezielt neu aufbauen (Pins und BLE sind geklärt)
    b) Komplett neu
25. **Grafikbibliothek:**
    a) ⭐ LVGL 9 (Wischgesten, Menüs, Animationen, schöne Diagramme; das S3 mit 8 MB PSRAM schafft das gut)
    b) TFT_eSPI wie bisher (schlanker, aber Menüs und Gesten sind Handarbeit)
26. **Wie soll Opus 5.5 liefern?**
    a) ⭐ In Etappen: erst Grundgerüst + Simulator, dann Seiten, dann Extras. Jede Etappe kompilierbar
    b) Alles in einem Durchgang
27. **Kannst du lokal kompilieren und Fehler zurückmelden** (VS Code + PlatformIO)? Die Cloud kann die Bibliotheken nicht laden, deshalb testen wir auf deinem Rechner. a) ⭐ Ja b) Ich brauche eine Anleitung

### K. Optionale Zusatz-Sensoren (Board hat beides nicht, nachrüstbar)

28. **Beschleunigungssensor MPU6050** an der I2C-Buchse (ca. 3 €): G-Kraft-Seite (Kurven, Bremsen), genauere 0–100-Messung, Fahrstil-Bewertung.
    a) ⭐ Im Bauplan als optionales Modul vorsehen, Firmware erkennt ihn automatisch, ohne Sensor wird die Seite ausgeblendet
    b) Jetzt fest einplanen (Sensor wird gekauft)
    c) Nicht vorsehen
29. **GPS NEO-6M/M8N** an GPIO 2/3, 3,3 V vom I2C-Stecker (ca. 8–15 €): Uhrzeit automatisch, Fahrtenbuch mit Start/Ziel und Strecke auf Karte (Export als GPX), Tempo-Abgleich mit dem Tacho.
    a) ⭐ Optionales Modul wie bei 28a
    b) Jetzt fest einplanen
    c) Nicht vorsehen
