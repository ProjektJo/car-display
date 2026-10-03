# Vorschläge für weitere Werte und Funktionen

Stand 3. Oktober 2026. ⭐ = meine Empfehlung. Aufwand: klein / mittel / groß (für die Firmware). Alles lässt sich aus den Daten berechnen, die wir ohnehin abfragen; wo ein Zusatzsensor nötig ist, steht es dabei.

## A. Neue Werte aus den vorhandenen Daten

| Nr. | Idee | Was du davon hast | Aufwand |
|---|---|---|---|
| 1 ⭐ | **Deine sparsamste Geschwindigkeit** | Die Firmware sammelt Verbrauch je Tempo (z. B. 50, 60, 70 … km/h, nur im höchsten Gang, ebene Strecke). Ergebnis: "Am sparsamsten fährst du mit 68 km/h: 4,6 l". Als kleine Kurve in Historie → Auswertung. | mittel |
| 2 ⭐ | **Verlorene Bremsenergie** | Jedes Bremsen vernichtet Bewegungsenergie, die vorher Sprit gekostet hat. Umgerechnet in Milliliter: "Durch Bremsen verloren: 0,18 l in dieser Fahrt". Ziel beim Hypermiling: diese Zahl klein halten. | klein |
| 3 | **Kosten pro Ampelstopp** | Anhalten und wieder auf 50 beschleunigen kostet messbar Sprit. Zähler "12 Stopps · 0,09 l" je Fahrt. | klein |
| 4 ⭐ | **Kaltstart-Anteil** | Wie viel Sprit in der Warmlaufphase verbraucht wurde und wie lange der Motor bis 80 °C braucht. Zeigt, warum Kurzstrecken teuer sind. | klein |
| 5 | **CO₂ pro Fahrt** | 2,37 kg CO₂ je Liter Benzin. Neben den Kosten in Fahrt & Tank. | klein |
| 6 | **Betriebsstunden und Leerlaufanteil** | Motorstunden gesamt, Anteil Leerlauf. Nützlich für Wartung (siehe 9). | klein |
| 7 | **Kennfeld Drehzahl × Last** | Farbiges Raster, in welchen Bereichen der Motor läuft. Der sparsamste Bereich liegt meist bei niedriger Drehzahl und mittlerer bis hoher Last. Für Technik-Interessierte. | mittel |

## B. Diagnose und Frühwarnung

| Nr. | Idee | Was du davon hast | Aufwand |
|---|---|---|---|
| 8 ⭐ | **Gemischkorrektur-Wächter** | Die Langzeit-Gemischkorrektur (LTFT) wird über Wochen mitgeschrieben. Driftet sie über ± 10 %, kommt ein Hinweis, noch bevor die Motorkontrollleuchte angeht. Typische Ursachen: Falschluft, Lambdasonde, Einspritzdüsen. | klein |
| 9 ⭐ | **Wartungserinnerung** | Ölwechsel und Inspektion nach km oder Monaten (km kommen aus unserer Streckenmessung, Monate nur mit GPS). Hinweis beim Start: "Ölwechsel in 450 km". | klein |
| 10 | **Thermostat-Check** | Erreicht das Kühlwasser nach 15 min Fahrt keine 80 °C, ist das Thermostat oft defekt (kostet Sprit). Einmaliger Hinweis. | klein |
| 11 | **Batterie und Lichtmaschine** | Ruhespannung vor dem Start (Zündung an, Motor aus) und Ladespannung im Verlauf. Hinweis bei schwacher Batterie (< 12,2 V Ruhe) oder Ladeproblem (< 13,2 V im Lauf). | klein |

## C. Funktionen

| Nr. | Idee | Was du davon hast | Aufwand |
|---|---|---|---|
| 12 ⭐ | **Fahrt-Zusammenfassung beim nächsten Start** | Weil der Strom mit der Zündung weg ist, kann am Fahrtende nichts angezeigt werden. Stattdessen beim nächsten Start 5 s lang: "Letzte Fahrt: 12,4 km · 5,8 l/100 · Score 78 · 1,29 €". | klein |
| 13 ⭐ | **Spar-Ziel** | Du setzt ein Ziel, z. B. 5,5 l/100 km für diese Tankfüllung. Auf der Eco-Seite erscheint eine Ziellinie in der Kurve, in der Historie der Fortschritt. | klein |
| 14 | **Strecken wiedererkennen (mit GPS)** | Gleiche Strecke (Arbeitsweg) wird erkannt, du siehst deinen besten Verbrauch und die beste Zeit auf genau dieser Strecke. | groß |
| 15 | **Bremsmessung 100–0** | Auf der Sprint-Ansicht: Bremsweg und Verzögerung (besser mit MPU6050). | klein |
| 16 | **Spritpreis-Verlauf** | Aus den eingegebenen Preisen: Verlauf und günstigster Preis der letzten Füllungen. | klein |

## D. Bedienung und Sicherheit

| Nr. | Idee | Was du davon hast | Aufwand |
|---|---|---|---|
| 17 ⭐ | **Automatisch zurück zur Eco-Seite** | Wenn du während der Fahrt 30 s nichts tippst und nicht auf Übersicht oder Großanzeige bist, springt das Display zurück zur Eco-Seite. Weniger Tippen beim Fahren. | klein |
| 18 ⭐ | **Fahrsperre für Menü und Eingaben** | Über 10 km/h sind Menü, Kachel-Belegung und Fehlercodes-Löschen gesperrt (Hinweis "Im Stand"). Lesen bleibt erlaubt. | klein |
| 19 | **Doppeltippen = Eco-Seite** | Von jeder Seite mit einem Doppeltipp zurück. | klein |
| 20 | **Helligkeit nach Sonnenstand (mit GPS)** | Tag/Nacht wechselt automatisch zum Sonnenuntergang am aktuellen Ort. | klein |
