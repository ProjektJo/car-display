# Etappe 7: Auswertung und Menü

## Was neu ist

- **Historie** mit vier Ansichten (Chips oben):
  - **Fahrten:** Balken mit Ø l/100 km der letzten 20 Fahrten, gestrichelt der Gesamtschnitt.
    - Farben: grün unter 95 % des Schnitts, bernstein über 110 %, sonst grau.
    - Tippen auf einen Balken zeigt darunter Fahrtnummer, Strecke, Ø, Dauer, Kosten, Gebremst und Eco-Score.
  - **Tankfüllungen:** runde Kurve Ø l/100 km je Füllung und gestrichelt € je 100 km. Darunter stehen die letzte Füllung, ihr Ø und die Veränderung seit der ersten gespeicherten Füllung.
  - **Auswertung:**
    - Ø nach Streckenlänge (< 5, 5–20, 20–50, > 50 km) mit dem Satz "Kurzstrecken unter 5 km brauchen … % mehr"
    - Summen der letzten 50 Fahrten
    - Eco-Score-Trend (vorige 10 → letzte 10 Fahrten)
    - beste Fahrt über 5 km
    - Schubanteil
  - **Spartempo:** l/100 km je Tempo 30–130 km/h, der sparsamste Punkt groß in Grün, darunter "Am sparsamsten bei 60 km/h · 3,8 l". Gezählt wird nur ruhige Konstantfahrt im höchsten Gang. Eine Tempoklasse zählt ab 5 km; vorher steht dort "?".
- **Fehlercodes:**
  - Gespeicherte (Mode 03) und vorläufige (Mode 07) Codes mit deutschem Klartext. Die Tabelle hat 309 generische P0-Codes (`data/dtc_de.csv`).
  - Unbekannte Codes: "Herstellerspezifischer Code".
  - "Neu lesen" liest die Codes neu, ebenso jedes Verbinden und jedes Öffnen der Seite (wenn älter als 60 s).
  - "Löschen" (Mode 04) geht nur bei stehendem Motor und nur nach einer Sicherheitsabfrage in Rot.
- **Info:** Kurzanleitung mit den Chips Bedienung, Farben, Werte und Eco-Score. Unten stehen Version, Profil, Adapter und Abfragen pro Sekunde.
- **Menü** mit allen Einträgen. Jede Zeile zeigt rechts den aktuellen Wert:
  - **Helligkeit:** Tag, Nacht oder Auto (nur mit GPS), dazu je eine Helligkeit mit − / +. Nacht dunkelt auch den Text ab.
  - **Spar-Ziel:**
    - Aus, Auto oder Fest, dazu −0,5 · − · Wert · + · +0,5.
    - Auto zeigt, woraus der Wert kommt ("6,2 → 5,7").
    - − oder + bei Auto macht daraus ein festes Ziel.
    - Mit Ziel ist die Ziellinie der Eco-Kurve grün, und Fahrt & Tank zeigt "Ø / Ziel".
  - **Fahrzeugart:** Kleinwagen bis Transporter mit Gewicht. Die Wahl gilt für Leistung, Bremsenergie und Eco-Score.
  - **Wartung:**
    - Tachostand einmal eintragen, danach zählt das Display selbst.
    - Ölwechsel und Inspektion mit "fällig in … km · alle … km" und "Erledigt"; unter 500 km in Bernstein.
    - Tippen auf eine der beiden Zeilen ändert das Intervall.
    - Ist eine Wartung fällig, zeigt die Start-Karte eine Zeile in Bernstein.
  - **Diagnose** wie bisher, neu ist "Mittelwerte zurücksetzen" (1 km, 10 km, 100 km, Tank oder alle).
  - **Getankt** von Hand, **Kacheln zurücksetzen**, **Spartipps** an/aus, **Kalt-Grenze** (2.000–3.000 U/min, Tippen schaltet weiter), **Auto-Sprint** an/aus, **Fahrt beenden**, **Info**.
- **Tempo-Tipp:** Wer länger als 30 s über 115 km/h fährt, sieht "100 statt 120 · –1,2 l/100 km". Die Ersparnis kommt aus der eigenen Spartempo-Statistik; ohne diese Daten erscheint der Tipp nicht.

## Was du prüfen sollst

Mit Board, Umgebung `simulator`:

1. Lang auf eine freie Fläche drücken: Das Menü zeigt zwölf Zeilen. Alle Unterdialoge öffnen und mit "Fertig" zurückgehen.
2. Helligkeit: "Nacht" wählen. Das Display wird dunkler und der Text etwas gedämpft. Danach wieder "Tag".
3. Spar-Ziel: "Auto" wählen. Auf der Eco-Seite erscheint die grüne Ziellinie mit "Ziel …".
4. Wartung: Tachostand eintragen (z. B. 123456). Danach steht dort "fällig in 15.000 km".
5. Fehlercodes: Nach etwa 3 min meldet der Simulator P0171 "Gemisch zu mager (Bank 1)". "Löschen" ist grau, solange der Motor läuft. Beim Tanken (etwa nach 10 min, Motor 30 s aus) lässt es sich löschen.
6. Historie: Nach ein paar Neustarts stehen mehrere Fahrten in den Balken. Tippen auf einen Balken zeigt seine Werte.
7. Info: die vier Chips durchtippen.

Im Auto (`freenove`):

8. Menü → Wartung → Tachostand eintragen.
9. Fehlercodes: Die Seite liest beim Verbinden. Liefert der Modus herstellerspezifische Codes (P1…), steht dort "Herstellerspezifischer Code".

## Annahmen

- **Wartung, Spar-Ziel auto und Spartempo** liegen in der Speicherdatei des Profils (LittleFS, A/B), nicht in NVS. Das schont den Flash, weil die km laufend mitzählen; außerdem gehören diese Werte zum Auto. Die Speicherdatei ist dafür auf Version 4 gestiegen, die Summen aus dem Simulator beginnen deshalb neu. Das Fahrtenbuch bleibt.
- **Standard-Intervalle:** Öl 15.000 km, Inspektion 30.000 km. Mit GPS kommen in Etappe 8 die Monate dazu.
- **Spartempo "ruhiges Gaspedal":** das Pedal bleibt 20 s lang in ± 5 Prozentpunkten.
- **Fehlercodes lesen:** beim Verbinden, beim Öffnen der Seite (wenn älter als 60 s) und mit "Neu lesen". Höchstens 8 gespeicherte und 8 vorläufige Codes.
- **Kalt-Grenze** gilt je Fahrzeugprofil (`cold_rpm_limit`). Helligkeit, Spar-Ziel, Spartipps und Auto-Sprint gelten für das ganze Gerät (NVS).
- **Helligkeit Auto (GPS)** ist ohne GPS ausgegraut und verhält sich bis Etappe 8 wie "Tag".
- **Kühlmittelgrenze** (kalt unter 60 °C) bleibt im Profil (`cold_coolant_c`) und hat keinen eigenen Menüpunkt.
- **"Sensor neu einlernen"** und **"Fahrten exportieren"** kommen mit MPU6050 und SD-Karte in Etappe 8.
