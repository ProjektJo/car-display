# Etappe 6: Sport und Sprint

## Was neu ist

- **Sport-Seite:**
  - Drehzahlbogen 0–6.500 U/min mit Skala ×1000. Ab 5.800 U/min wird er bernstein, dort liegt auch der schwache Rotbereich.
  - In der Mitte Tempo und Gang, mit grünem ▲ zum Hochschalten.
  - Vier Live-Balken: Leistung (kW und PS, Skala bis zur Nennleistung des Profils), Beschleunigung (Balken um die Mitte, bremsen grau), Gaspedal und Saugrohrdruck.
  - Live-Diagramm der letzten 30 s mit jeder Abfrage. Der Chip rechts schaltet das Paar um: Tempo + Leistung, Drehzahl + Gas, Beschleunigung + Leistung. Die Wahl wird gespeichert.
  - Läuft eine Messung, steht über dem Diagramm "0–100 läuft · 3,8 s".
- **Sprint-Seite:**
  - Stoppuhr 0–100 mit "bereit", "läuft" oder "geschafft", Zeit groß und Fortschrittsbalken bis 100 km/h.
  - Tempoverlauf der letzten Messung (blau) und der besten (grün gestrichelt).
  - Tabelle 0–50, 0–100 und 80–120 km/h mit letzter und bester Zeit.
  - Unten Ø Fahrt, Zeit pro 100 km, Vmax und Spitzenleistung der Fahrt.
- **Messregeln** (Architektur Kapitel 10):
  - Die Messung startet beim Losrollen.
  - Gültig wird sie nur, wenn innerhalb von 3 s ein Sprint erkannt wird: mindestens 1 s gestanden, dann Gaspedal ab 85 % und Drehzahl ab 3.000 U/min. Normales Anfahren wird still verworfen.
  - Abbruch, wenn das Gas länger als 1,5 s unter 50 % bleibt (Schalten geht schneller) oder das Tempo um mehr als 3 km/h fällt.
  - 80–120 startet beim Durchfahren von 80 km/h mit Vollgas.
  - Die Zeiten sind zwischen zwei Abfragen interpoliert.
- **Auto-Sprint:** Ein erkannter Sprint holt die Sprint-Seite nach vorn. Zurück geht es nach diesen Regeln:
  - 4 s nach dem Ziel
  - 2 s nachdem das Gas unter 50 % fällt
  - wenn 3 s nach dem Wechsel keine Messung läuft
  - spätestens nach 25 s

  Wischst du selbst oder öffnest ein Fenster, bleibt die Seite stehen.
- **Abfragen:** Während einer Messung fragt die Firmware nur Tempo, Drehzahl und Gaspedal ab. Das gibt mehr Messpunkte.
- **Beste Zeiten** und die beste Kurve gelten je Fahrzeugprofil und überleben das Ausschalten.

## Was du prüfen sollst

Mit Board, Umgebung `simulator`:

1. Alle 4 Minuten startet der Simulator an einer Ampel eine Vollgas-Sequenz. BOOT 2 s gedrückt halten startet sie beim nächsten Halt. Die Anzeige wechselt von selbst zur Sprint-Seite: zuerst "läuft", dann "geschafft" mit etwa 13–14 s. Nach 4 s springt sie zur vorigen Seite zurück.
2. Auf der Sprint-Seite stehen danach 0–50 und 0–100 in der Tabelle. Die grüne Kurve ist die beste Messung.
3. Sport-Seite: Der Chip "— Tempo - - Leistung" schaltet das Diagramm um.
4. Während eines Sprints selbst wischen: Die Anzeige bleibt dann auf der gewählten Seite.

Im Auto (`freenove`):

5. Eine 0–100-Messung nur dort, wo es sicher und erlaubt ist. Normales Anfahren darf nie in der Tabelle landen.

## Annahmen

- **Startzeit:** die letzte Abfrage mit stehendem Auto, wie in der Vorschau. Bei 4–8 Abfragen pro Sekunde liegt sie bis zu 0,25 s zu früh. Mit GPS oder MPU6050 (Etappe 8) wird das genauer.
- **Leistung:** aus der Tempoänderung mit Fahrzeugart (Gewicht, cw·A). Negative Werte (bremsen) zeigt der Balken als 0. Die Spitzenleistung zählt nur mit Gas.
- **Beste Zeiten** liegen in der Speicherdatei des Profils (LittleFS, A/B mit Prüfsumme), nicht in NVS. Dort sind sie genauso stromausfallsicher, und alles eines Profils liegt beisammen.
- **Kurve:** gespeichert als Zeitpunkte, zu denen 0, 5, 10 … 100 km/h erreicht wurden.
- **"geschafft"** bleibt 10 s stehen, danach zeigt die Karte wieder "bereit" mit der letzten Zeit.
- **Auto-Sprint** ist an. Der Schalter im Menü kommt in Etappe 7.
