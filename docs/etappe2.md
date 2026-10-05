# Etappe 2: Verbindung

## Was neu ist

- **Bluetooth:** Die Firmware sucht den Adapter automatisch, verbindet sich und findet den Datendienst selbst. Den Code dafür habe ich aus der alten Firmware übernommen.
- **ELM327-Client:** Der Adapter wird mit ATZ, ATE0, ATL0, ATS0, ATH0, ATSP0 und ATAT2 eingestellt. Der eigene Antwort-Parser versteht auch mehrere PIDs in einer Antwort, CAN-Antworten über mehrere Rahmen und Antworten von mehreren Steuergeräten.
- **Fahrzeugdaten:** Nach dem Verbinden liest die Firmware die Liste der unterstützten PIDs (0100, 0120 …) und, falls das Auto sie liefert, die VIN.
- **PID-Scheduler:** Er fragt nach Takt-Klassen ab:
  - schnell: jede Runde
  - mittel: etwa jede Sekunde
  - langsam: etwa alle 5 s
  - selten: alle 30 s

  PIDs, die das Auto nicht kennt, fragt er nie ab. Auf CAN gehen bis zu 6 PIDs in eine Anfrage, auf KWP immer nur einer.
- **Neuverbindung:** Nach einem Fehler versucht die Firmware es nach 1, 2, 5 und 10 s erneut, danach alle 10 s.
- **Startbildschirm:** Er zeigt die Schritte der Verbindung und bei einem Fehler einen Satz mit der Lösung.
- **Menü → Diagnose:** Dort stehen Adapter, Protokoll, VIN, Abfragen pro Sekunde, unterstützte PIDs, woraus der Verbrauch gerechnet wird und die Fahrzeugart.
- **Simulator:** Er spielt den Verbindungsaufbau nach, damit du Startbildschirm und Diagnose auch ohne Auto siehst.

So sieht das am PC aus: [pc-vorschau-etappe2.png](pc-vorschau-etappe2.png).

## Was du prüfen sollst

Ohne Board:

1. `freenove` und `simulator` bauen. Schick mir bitte die Zeilen `RAM:` und `Flash:` von beiden Builds. Durch Bluetooth wachsen beide Werte deutlich.

Mit Board, Umgebung `simulator`:

2. Der Startbildschirm zeigt nacheinander drei Schritte und verschwindet etwa 1,5 s nach "Fahrzeug: Renault Modus".
3. Lang drücken öffnet das Menü. Ein Tippen auf "Diagnose" öffnet den Dialog, dort sollte "8,0 pro Sekunde" und bei VIN "nicht geliefert" stehen. "Fertig" führt zurück ins Menü.

Mit Board und Adapter im Auto, Umgebung `freenove`, Zündung an:

4. Den seriellen Monitor offen lassen und die Zeilen ab `BLE: Suche Adapter …` bis `OBD: läuft` an mich schicken. Besonders wichtig ist die Zeile mit den unterstützten PIDs: Sie zeigt, ob der Modus Tankfüllstand (2F) und Gaspedal (49) liefert.
5. Auf der Übersicht sollten Tempo, Drehzahl und die übrigen Werte laufen. Menü → Diagnose zeigt, wie viele Abfragen pro Sekunde das Auto schafft (erwartet: 4–8), und ob es die VIN liefert.
6. Zündung aus: Der Punkt links oben wird grau, die Werte zeigen "–". Zündung wieder an: Nach höchstens 10 s laufen die Werte wieder.
7. Handy-App des Adapters verbinden und dann das Display einschalten: Es sollte "Adapter lässt sich nicht verbinden" bzw. "Kein Adapter gefunden" mit der Lösung zeigen.

## Annahmen

- **Startbildschirm:** Er erscheint nur bis zur ersten Verbindung nach dem Einschalten. Reißt die Verbindung beim Fahren ab, zeigt das nur der graue Punkt, die Seite bleibt stehen.
- **Ausblenden:** Tippen, Wischen oder die BOOT-Taste blenden den Startbildschirm aus, damit du die Seiten auch ohne Auto ansehen kannst.
- **Auto weg:** Nach 5 Abfragen hintereinander ohne Antwort gilt das Auto als weg (Zündung aus). Die Firmware sucht das Protokoll dann neu.
- **Protokollsuche:** Die erste Abfrage darf bis zu 15 s dauern, weil KWP mit 5-Baud-Init so lange braucht.
- **Gaspedal und Drosselklappe:** Liefert das Auto das Gaspedal (49), ist es schnell getaktet und die Drosselklappe (11) nur im Sekundentakt. Ohne Gaspedal ist die Drosselklappe schnell.
- **ATAT2:** Kennt ein Adapter-Klon ATAT2 nicht, geht es ohne adaptives Timing weiter.
- **Menü:** Es enthält bis Etappe 7 nur die Zeile "Diagnose".
- **Standardprofil:** Bis zu den Fahrzeugprofilen in Etappe 3 gilt immer "Renault Modus", Kleinwagen mit 1.150 kg und Kalibrierung 1,00.
- **Eigenes Layout:** Die Vorschau hat keinen Startbildschirm, deshalb habe ich ihn schlicht im Stil der übrigen Seiten gestaltet.

## Nachtrag (5. Oktober)

- **VIN im Diagnose-Dialog:** Auf deinen Wunsch steht die VIN jetzt in der Diagnose. Liefert das Auto sie nicht, steht dort "nicht geliefert". Der Simulator liefert keine VIN, wie vermutlich der Modus.
