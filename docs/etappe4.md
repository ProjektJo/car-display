# Etappe 4: Eco-Seite

## Was neu ist

- **Eco-Seite** (Startseite):
  - **Momentanverbrauch** groß, eingefärbt nach Bezug: grün unter 95 %, bernstein über 110 %. Bezug ist das Spar-Ziel, ohne Ziel der Tank-Schnitt. Im Schub steht "SCHUB 0,0" in Grün, im Stand der Verbrauch in l/h mit "Stand".
  - **Eco-Kurve** durch fünf Punkte: Tank, 100 km, 10 km, 1 km und Momentan. Die Linie ist rund, darunter liegt eine schwache Fläche. Die gestrichelte Linie ist der Tank-Schnitt; mit Spar-Ziel ist sie grün (das Ziel stellst du ab Etappe 7 im Menü ein). Werte über 12 stehen oben mit ↑.
  - **Gang** rechts oben mit grünem ▲, sobald die Schaltempfehlung 1 s anliegt.
  - **Untere Leiste:** Eco-Score, Schub gespart und Gebremst der laufenden Fahrt.
- **Gänge lernen:** Die Firmware sammelt in ruhigen Phasen das Verhältnis Tempo zu Drehzahl. Daraus findet sie die Gänge und speichert sie im Fahrzeugprofil. Bis dahin zeigt der Gang nur "N" im Stand bzw. beim ausgekuppelten Rollen, sonst "–".
- **Schaltempfehlung:** ab der Schaltdrehzahl des Profils (Benziner 2.200 U/min), bei weniger als 60 % Gas, nicht im höchsten Gang und nur, wenn der nächste Gang noch mindestens 1.300 U/min hat.
- **Hinweis-Feld** links neben dem Gang, nur im akuten Fall und höchstens 8 s:
  - Gang rein · Schub 0 l (ausgekuppelt bremsen)
  - Sanfter Gas geben
  - Früher vom Gas
  - Stand 1:30 · Motor aus? (nach 60 s Stand, dann alle 30 s)
  - Gleichmäßig Gas halten
  - Motor 45 °C · max. 2.500 U/min (bernstein, solange der Motor kalt ist und zu hoch dreht)

  Zwischen zwei Tipps liegen mindestens 60 s, derselbe Tipp kommt frühestens nach 5 min wieder. Während Schub, solange ein Fenster offen ist und 3 s nach einem Tippen gibt es keine Tipps.
- **Eco-Score** mit den fünf Teilen und Gewichten aus der Architektur (Rollen 35 %, ruhiges Gas 25 %, früh schalten 20 %, sanft bremsen 10 %, wenig Leerlauf 10 %). Er wird im Fahrtenbuch gespeichert.
- **Bremsenergie** ("Gebremst"): was über das natürliche Ausrollen hinaus verzögert wird, umgerechnet in den Sprit, den die Bewegung gekostet hat.
- **Schub gespart:** Schubzeit × Leerlaufverbrauch. Den Leerlaufverbrauch lernt die Firmware im warmen Stand, bis dahin rechnet sie mit 0,7 l/h.
- **Start-Karte "Letzte Fahrt"** nach dem Start: Fahrt, Strecke, Dauer, Ø, Kosten, Eco-Score, Gebremst. Sie schließt nach 6 s, beim Losfahren oder durch Tippen. Ist die letzte Fahrt kürzer als 1 km, kommt sie nicht.
- **Fahrzeug-Prüfung:** Passen nach 2 min ruhiger Fahrt höchstens ein Drittel der Phasen zu den gelernten Gängen, fragt das Display einmal "Fährst du mit Fahrzeug …?". "Ja" behält das Profil und lernt die Gänge neu, "Fahrzeug ändern" öffnet "Welches Fahrzeug?".
- **Übersicht:** Die Drehzahl-Kachel wird bernstein, wenn der Motor kalt ist und zu hoch dreht.
- **Simulator:** Stadt fährt jetzt im 4. Gang, der Fuß zittert realistischer, und am Ende des Zyklus steht das Auto 65 s (Stand-Hinweis). Dort startet keine Vollgas-Sequenz.

## Was du prüfen sollst

Mit Board, Umgebung `simulator` (läuft schon auf dem Board):

1. Nach dem Start erscheint die Karte "Letzte Fahrt" und schließt nach 6 s oder durch Tippen.
2. Die Eco-Seite zeigt Momentanverbrauch, Kurve, Gang und die untere Leiste. Beim Beschleunigen erscheint kurz das grüne ▲ vor dem Gang.
3. In den ersten Minuten ist der Motor kalt: Das Hinweis-Feld zeigt bernstein "Motor … °C · max. 2.500 U/min", solange die Drehzahl über 2.500 liegt.
4. Im Schub steht "SCHUB 0,0" in Grün und der rechte Punkt der Kurve liegt auf 0.
5. Gegen Ende des Zyklus (ausgekuppelt bremsen bis zum Stand) kommt "Gang rein", danach im langen Stand nach 60 s "Stand 1:00 · Motor aus?".
6. Nach ein paar Minuten zeigt der Gang 2 bis 5 statt nur "N" und "–". Im Monitor steht dann `Profil gespeichert: … 4 Gänge: 13.2 19.5 26.0 32.0`. Der 1. Gang fehlt in der Simulation, weil er nie lange ruhig gefahren wird. Die Zählung beginnt dann bei 2.

Mit Board und Adapter im Auto, Umgebung `freenove`:

7. Eine gemischte Fahrt von 20–30 min: Danach sollte der Gang stimmen. Bitte schick mir die Zeile `Profil gespeichert: …` aus dem Monitor.
8. Prüfe, ob das ▲ zu früh oder zu spät kommt. Die Schaltdrehzahl steht im Profil (`shift_rpm`, Standard 2.200).

## Werkzeuge (neu)

- **Unit-Tests auf dem Board:** Auf diesem PC ist kein g++ installiert, deshalb laufen die Tests der Rechenmodule jetzt auch direkt auf dem Board: `pio test -e board_test`. Jede Testgruppe wird einzeln geflasht (etwa 1 min je Gruppe). `pio test -e native` geht weiterhin, wenn g++ installiert ist.
- **Bildschirmfoto über USB:** `python tools/screenshot.py COM7 bild.png` holt ein Foto vom Display (nur zur Fehlersuche, abschaltbar mit `SCREENSHOT_SERIAL` in `config.h`). Achtung: Das Öffnen des seriellen Anschlusses startet das Board neu.
- **Partitionstabelle:** Die App liegt jetzt bei 0x10000 (vorher 0x20000, deshalb blieb das Display schwarz). Bitte nicht wieder verschieben.

## Annahmen

- **Gang-Histogramm:** Klassen von je 1 % zwischen k = 3 und 60 (km/h je 1000 U/min). Ein Gang braucht mindestens 20 ruhige Proben und 2 % aller Proben; zwei Gänge liegen mindestens 12 % auseinander. Ins Profil kommen die Gänge erst nach 30 s ruhiger Fahrt insgesamt.
- **1. Gang:** Ist der kleinste gelernte Wert größer als 10,5, fehlt der 1. Gang. Die Zählung beginnt dann bei 2.
- **Leerlauf:** Drehzahl unter 1.000 U/min bei mehr als 15 km/h zählt als "N" (ausgekuppelt).
- **Gaspedal "zu":** höchstens 3 Prozentpunkte über dem kleinsten Wert seit dem Start. Ohne Gaspedal-PID (0x49) gilt die Drosselklappe.
- **Ruhiges Gas:** Das Pedal wird mit τ = 0,3 s geglättet, die Beschleunigung aus dem Tempo mit τ = 0,5 s.
- **"In Bewegung"** ab 3 km/h. Bremsenergie zählt nur ohne Gas.
- **Gleichmäßig Gas halten** erst ab 30 km/h, sonst schlägt die Regel im Stop-and-go an.
- **Sanfter Gas geben:** Ohne Beschleunigungssensor lässt sich ein Überholvorgang nicht erkennen. Der Tipp kommt deshalb auch beim Überholen (höchstens alle 5 min).
- **Fahrzeug-Prüfung:** Bis zur Antwort lernt die Firmware keine Gänge dazu. Schließt das Fenster ohne Antwort (60 s), bleibt alles, wie es ist, und es wird bis zum nächsten Einschalten nicht mehr gefragt.
- **Start-Karte:** Die Hinweiszeile für Wartung und Thermostat kommt mit Etappe 7 bzw. 8.
- **Spartipps an/aus** kommt mit dem Menü in Etappe 7, bis dahin sind sie an. Der Tempo-Tipp ("100 statt 120") braucht die Spartempo-Statistik und kommt in Etappe 7, der Thermostat-Hinweis in Etappe 8.
- **Schriften:** Für den Momentanverbrauch nehme ich die in LVGL eingebaute Montserrat 38, für die zweite Zeile im Hinweis-Feld Montserrat 10 (beide nur ASCII; "–" kommt aus der eigenen 28er). Eine eigene 38er-Schrift mit Umlauten bräuchte Node.js zum Erzeugen, das auf diesem PC fehlt.
- **Kurve:** Die Fläche unter der Kurve ist gleichmäßig schwach statt mit Verlauf.
- **Speicherdatei:** Version 2 (Gänge, Leerlaufverbrauch, letzte Fahrt). Gespeicherte Summen aus Etappe 3 werden beim ersten Start verworfen. Am Auto ist noch nichts gefahren, es geht also nichts verloren.
