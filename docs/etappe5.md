# Etappe 5: Alltagsseiten

## Was neu ist

- **Übersicht:** Die sechs Kacheln sind frei belegbar.
  - **Lang drücken** (0,8 s) auf eine Kachel öffnet "Kachel N: Wert wählen" mit allen Werten: Tempo, Drehzahl, Momentan, Ø 10 km, Ø 100 km, Ø Tank, Ø seit Profil, Gaspedal, Reichweite, Kühlmittel, Bordspannung, Motorlast, Saugrohr, Ansaugluft und Gang. Die Wahl bleibt nach dem Ausschalten erhalten.
  - **Tippen** auf eine Kachel zeigt den Verlauf der letzten 5 min mit Min und Max. Ein weiteres Tippen schließt ihn. Für Mittelwerte über die Strecke gibt es keinen Zeitverlauf, dort steht ein kurzer Satz.
  - Die Reichweite hat überall denselben geteilten Balken: gefahren seit dem Tanken (grau) | Rest (blau, unter 50 km bernstein). Darunter stehen "⛽ ··· 316" (km seit dem Tanken) und "Σ 763 km" (ganze Tankfüllung).
- **Großanzeige:** ein Wert in 72 px. Mit den Pfeilen links und rechts schaltest du durch Tempo, Momentan, Ø 10 km, Drehzahl, Reichweite und Kühlmittel. Die Punkte unten zeigen, wo du bist. Die Wahl wird gespeichert.
- **Fahrt & Tank:** links die Fahrt (Strecke, Dauer, Ø ab 0,5 km, Kosten, Leerlauf, Eco-Score), rechts die Tankfüllung (gefahren, verbraucht, im Tank, Reichweite, Ø-Preis im Tank, Kosten je 100 km bzw. "Ø / Ziel", wenn ein Spar-Ziel gesetzt ist). Darunter die Reichweite mit Balken und der Knopf **Getankt**.
- **Tank-Fenster:**
  - links die Liter, rechts der Preis pro Liter
  - je Stelle ein ▲ und ein ▼; gedrückt halten wiederholt, der Überlauf rechnet weiter
  - Tippen auf die Ziffern öffnet ein Ziffernfeld; beim Preis nur Euro und Cent, die ⁹ kommt immer dazu (1,79 → 1,799 €/l)
  - Schalter "vollgetankt" bzw. "nicht voll"
  - die Kosten rechnen live mit
  - "Nicht getankt" verwirft, "OK" speichert

  Vorausgefüllt sind der letzte Zapfsäulenpreis und die geschätzten Liter. Oben rechts steht, woher sie kommen: aus dem Verbrauch, über die Tankanzeige oder vom Beleg, sobald du sie änderst. Das Fenster schließt nicht von selbst.
- **Automatische Tankerkennung** (nur wenn das Auto den Füllstand 0x2F liefert): Beim Motorstart mittelt die Firmware den Füllstand 10 s im Stand. Ist er um mindestens 8 % der Tankgröße gestiegen, öffnet sich das Tank-Fenster von selbst, egal auf welcher Seite.
- **Diagramme:** Verlauf der letzten 1, 5 oder 30 min (Chips oben rechts). Unten wählst du bis zu zwei Werte: Verbrauch, Tempo, Drehzahl, Gas, Kühlmittel, Spannung. Der zweite Wert ist gestrichelt und hat die rechte Achse. Die Achsen haben runde Werte. Tempo, Drehzahl, Gas und Verbrauch beginnen bei 0. Die Wahl wird gespeichert. Der Verlauf beginnt mit dem Einschalten (1 Wert je Sekunde, 30 min).
- **Aufgeräumt:** Die Fehlersuch-Umgebungen `hwtest…` und `simulator_langsam` sind wieder entfernt.

## Was du prüfen sollst

Mit Board, Umgebung `simulator`:

1. **Übersicht:** Eine Kachel lang drücken und z. B. "Kühlmittel" wählen. Board aus- und wieder einschalten: Die Belegung ist noch da.
2. **Übersicht:** Auf "Tempo" tippen. Es erscheint der Verlauf der letzten 5 min mit Min und Max. Tippen schließt.
3. **Großanzeige:** mit den Pfeilen durchschalten. Bei "Reichweite" steht der Balken darunter.
4. **Fahrt & Tank:** "Getankt" antippen. Liter und Preis mit ▲/▼ ändern und dann auf die Preisziffern tippen. Im Ziffernfeld "1,85" eingeben und "Übernehmen" drücken. Danach steht 1,85⁹ da. Mit "OK" speichern: Der Ø-Preis im Tank ändert sich, und der Monitor schreibt `Getankt: … l zu 1.859 EUR/l`.
5. Der Simulator tankt nach etwa 7,5 min mit abgestelltem Motor. Beim Weiterfahren öffnet sich das Tank-Fenster von selbst ("Liter geschätzt über Tankanzeige").
6. **Diagramme:** 1/5/30 min umschalten und Werte an- und abwählen.

Im Auto (`freenove`):

7. Nach dem nächsten Tanken mit Beleg: Liter vom Beleg eintragen, Preis prüfen, OK. Ab der zweiten Vollbetankung mit Beleg-Litern kalibriert sich der Verbrauch.

## Annahmen

- **Preisvorschlag:** Wurde noch nie ein Preis eingegeben, schlägt das Tank-Fenster 1,79⁹ €/l vor.
- **Liter-Vorschlag von Hand ("Getankt"):** der berechnete Verbrauch seit der letzten Tankfüllung.
- **Tankerkennung:** Fährt das Auto los, bevor 10 s gemittelt sind, entscheidet die Firmware mit den bis dahin gemessenen Werten. Nach dem Speichern im Tank-Fenster wird der Füllstand neu gelernt, damit derselbe Tankvorgang nicht zweimal erkannt wird.
- **Eingabegrenzen:** Liter bis 99,9, Preis bis 9,99⁹ €/l.
- **Detail-Verlauf:** nur für Werte mit Zeitverlauf (Tempo, Drehzahl, Momentan, Gaspedal, Kühlmittel, Spannung). Die Mittelwerte und die Reichweite zeigen stattdessen einen Satz.
- **Verlauf "Verbrauch":** in l/100 km, im Schub 0, im Stand kein Wert, über 20 abgeschnitten (wie in der Vorschau).
- **Fahrtenbuch und Tankliste** öffnen sich mit der Historie-Seite in Etappe 7, bis dahin hat das Tippen auf die Spalten von Fahrt & Tank keine Wirkung.
- **Seitenwechsel bei offenem Fenster:** Ein offenes Detail läuft weiter, auch wenn sich darunter die Seite ändert.
