# Etappe 3: Rechnen und Speichern

## Was neu ist

- **Verbrauch:** Die Firmware rechnet den Momentanverbrauch aus 0x5E, sonst aus dem Luftmassenmesser, sonst über Saugrohrdruck und Drehzahl (Speed-Density, beim Modus wahrscheinlich). Unter 5 km/h zeigt sie l/h, darüber l/100 km. Im Schub ist der Verbrauch genau 0. Das erkennt die Firmware am Kraftstoffsystem (0x03), ohne diesen Wert an: Gas zu, über 1.200 U/min und über 15 km/h.
- **Mittelwerte:** Ø 1 km, Ø 10 km und Ø 100 km laufen als Strecken-Fenster mit, dazu Ø Fahrt, Ø Tank (in den ersten 30 km nach dem Tanken der Wert der vorigen Füllung), Ø Profil und Ø der letzten 5 Tankfüllungen.
- **Tank und Reichweite:** Liefert das Auto den Füllstand (0x2F), kommt der Tankinhalt daher, geglättet. Sonst rechnet das Tankmodell ab der letzten Vollbetankung. Die Reichweite nimmt einen gewichteten Prognose-Verbrauch aus Ø 100 km, Ø 10 km und den letzten Füllungen. Unter 80 km rechnet sie vorsichtig mit dem höchsten dieser Werte.
- **Kalibrierung, Mischpreis, Kosten:** Diese Rechnungen sind fertig und getestet. Bedienen kannst du sie erst mit dem Tank-Fenster in Etappe 5.
- **Speichern:** Die laufenden Summen landen jede Minute und bei jedem Halt über 10 s im Flash. Es gibt zwei Dateien mit Prüfsumme, die abwechselnd geschrieben werden: Fällt der Strom beim Schreiben aus, gilt die andere. Fahrten (letzte 50) und Tankfüllungen (letzte 100) kommen in eigene Dateien.
- **Fahrtende:** War der Motor beim Abstellen warm (mindestens 70 °C) und ist er beim nächsten Start höchstens 4 °C kälter, läuft die Fahrt weiter (Tanken, Bäcker). Sonst beginnt eine neue. Steht der Motor 5 min, endet die Fahrt auch ohne Neustart.
- **Fahrzeugprofile:** Nach dem Verbinden sucht die Firmware das passende Profil. Zuerst zählt die VIN, sonst die PID-Liste mit dem Protokoll (deine Entscheidung vom 5. Oktober). Passt keins oder passen mehrere, fragt das Display "Welches Fahrzeug?". Gibt es noch gar kein Profil, öffnet sich gleich der Assistent "Neues Fahrzeug" mit Name, Kraftstoff, Hubraum und Tankgröße. Er ist mit dem Modus vorausgefüllt (1,2 l, Benzin, 49 l).
- **Übersicht:** Die Kacheln zeigen jetzt die Standardbelegung aus dem UI-Entwurf: Tempo, Drehzahl, Momentan, Ø 10 km, Gaspedal, Reichweite. Frei belegbar werden sie in Etappe 5.
- **Diagnose:** Die erste Zeile heißt "Fahrzeug" und zeigt das geladene Profil. Ein Tippen darauf öffnet "Welches Fahrzeug?".

So sieht das am PC aus: [pc-vorschau-etappe3.png](pc-vorschau-etappe3.png). Oben siehst du den Assistenten beim ersten Start, die Tastatur für den Namen und die Auswahl. Unten sind die Diagnose und zweimal die Übersicht während der simulierten Fahrt.

## Was du prüfen sollst

Ohne Board:

1. `freenove` und `simulator` bauen. Schick mir bitte die Zeilen `RAM:` und `Flash:` von beiden Builds.
2. `pio test -e native` laufen lassen. Neu sind die Tests `test_fuel`, `test_averages`, `test_trip`, `test_profile`, `test_profile_json` und `test_vehicle_calc`. Beim ersten Mal lädt PlatformIO dafür ArduinoJson herunter. Bitte die letzte Zeile mitschicken (alle Tests, 0 Failures).

Mit Board, Umgebung `simulator`:

3. Beim ersten Start öffnet sich nach dem Startbildschirm der Assistent "Neues Fahrzeug". Auf "Speichern" tippen. Der Startbildschirm zeigt dann "Fahrzeug: Renault Modus".
4. Auf der Übersicht laufen alle sechs Kacheln. Ø 10 km und Reichweite erscheinen nach dem ersten gefahrenen Kilometer (etwa 2–3 min).
5. Im seriellen Monitor steht etwa jede Minute eine Zeile `Gespeichert: Fahrt … km, Ø 10 km …`.
6. Board nach ein paar Minuten vom Strom nehmen und wieder anstecken. Es darf nicht mehr fragen, und Ø 10 km zeigt sofort wieder einen Wert. Der Monitor schreibt `Profil: Renault Modus geladen (mit gespeicherten Summen)` und `Fahrtenbuch: Fahrt 1 mit … km gespeichert`. Der Simulator startet immer kalt, deshalb beginnt eine neue Fahrt.
7. Menü → Diagnose → "Fahrzeug" antippen, dann "+ Neues Fahrzeug". Den Namen antippen, mit der Tastatur ändern, ✓ und "Speichern". Die Diagnose zeigt das neue Profil, und nach einem Neustart lädt es sich von selbst. Das alte Profil passt dann nicht mehr automatisch zu diesem Auto.

Mit Board und Adapter im Auto, Umgebung `freenove`:

8. Beim ersten Verbinden fragt das Display einmal. Den Assistenten mit "Speichern" bestätigen, danach nie wieder. Der Simulator legt seine Profile getrennt ab und stört das echte Profil nicht.
9. Eine Runde fahren und die Werte auf der Übersicht mit dem Bordcomputer vergleichen, falls der Modus einen hat. Vor der ersten Kalibrierung sind ±15 % normal.
10. Im seriellen Monitor die Zeilen `Gespeichert: …` einer Fahrt an mich schicken.

## Annahmen

- **Frage ohne Wahl schließen:** Wer "Welches Fahrzeug?" mit "Fertig" oder "Abbrechen" schließt, fährt mit dem vorher geladenen bzw. zuletzt benutzten Profil weiter. Gibt es noch keins, entsteht eins mit den Standardwerten ("Renault Modus").
- **Ein Auto, ein Profil:** Wählst du ein Profil von Hand oder legst eins an, merkt es sich VIN, PID-Liste und Protokoll dieses Autos. Andere Profile, die bisher dazu passten, vergessen sie. So fragt das Display nicht bei jedem Start, und deine Wahl gilt auch beim nächsten Mal.
- **Neu verbinden während der Fahrt:** Ist das Auto einmal einem Profil zugeordnet, bleibt das Profil bis zum Ausschalten, auch wenn die Verbindung kurz abreißt. Es wechselt nur, wenn das Auto eine andere VIN meldet. Ein Funkloch löst also keine Frage mitten in der Fahrt aus.
- **Speichern:** Jede Minute wird nur geschrieben, wenn sich Kilometer oder Liter geändert haben. Das schont den Flash, falls das Display dauerhaft Strom hat.
- **Ohne Verbrauchswert** (Quelle fehlt oder Werte kurz veraltet) zählen diese Sekunden nicht in die Schnitte. Bei einem Diesel ohne 0x5E zeigen alle Schnitte deshalb "–" statt 0,0.
- **Höchstens 8 Profile**, Namen bis 16 Zeichen. Die Tastatur hat Umlaute und ß.
- **Leistung** fragt der Assistent nicht ab. Sie wird aus dem Hubraum geschätzt (46 kW je Liter) und skaliert nur den Leistungsbalken in Etappe 6.
- **Gaspedal-Kachel:** Liefert das Auto kein Gaspedal (0x49), zeigt sie die Drosselklappe (0x11).
- **Füllstand:** Der Wert aus 0x2F wird über 30 s geglättet, weil er beim Fahren schwappt. Nach dem Tanken steigt er deshalb erst langsam. Die automatische Tankerkennung kommt in Etappe 5.
- **Fenster-Mittelwerte** zeigen erst einen Wert, wenn 10 % ihrer Länge gefahren sind (Ø 1 km ab 100 m, Ø 10 km ab 1 km, Ø 100 km ab 10 km). Fahrt-, Tank- und Profil-Schnitt zeigen ab 1 km einen Wert.
- **Ø der letzten Füllungen** ist Summe Liter durch Summe km, nicht der Mittelwert der Einzelwerte. Eine kurze Füllung zählt so weniger.
- **Kalibrierung** gilt für alle Verbrauchsquellen, auch für 0x5E.
- **Kosten** einer Fahrt werden mit dem Mischpreis gerechnet, der während der Fahrt im Tank war.
- **Ohne Kühlmitteltemperatur** (0x05 fehlt) beginnt nach jedem Start eine neue Fahrt. Fahrten unter 100 m kommen nicht ins Fahrtenbuch.
- **Tankmodell ohne 0x2F:** Bis zur ersten Vollbetankung ist der Tankinhalt unbekannt, Reichweite zeigt dann "–".
- **Ansauglufttemperatur:** Fehlt sie (0x0F), rechnet Speed-Density mit 25 °C.
- **Eigenes Layout:** Für "Welches Fahrzeug?", den Assistenten und die Tastatur hat die Vorschau kein Layout. Ich habe sie im Stil von Menü und Diagnose gebaut.
- **Schriften:** `font_m12`, `font_m14` und `font_m20` sind neu erzeugt, damit die Rücktaste der Tastatur ein Symbol hat.

## Gang-Prüfung (deine Antwort vom 5. Oktober)

Kommt in Etappe 4: Passen die Gänge einer Fahrt nicht zum geladenen Profil, fragt das Display einmal "Fährst du mit Fahrzeug …?" mit "Ja" und "Fahrzeug ändern". Steht in `docs/plan/architektur.md` Kapitel 6.
