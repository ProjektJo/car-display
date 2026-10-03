# Car-Display

ESP32 liest per BLE-OBD-Adapter (ELM327-kompatibel) OBD2-Daten aus dem Auto und zeigt Werte und Verläufe,
die das Kombiinstrument nicht verrät: echte Kühlmitteltemperatur, Bordspannung,
Ansauglufttemperatur, Motorlast, Momentanverbrauch, Gemischkorrektur.

## Hardware

| Teil | Empfehlung | Hinweis |
|---|---|---|
| Board + Display | **Freenove ESP32-S3 Display 2,8" IPS Touch** (FNK0104B: ILI9341 240x320, FT6336U-Touch, 16 MB Flash, 8 MB PSRAM) | Pins aus Freenoves eigener TFT_eSPI-Konfiguration übernommen |
| OBD-Adapter | **BLE / Bluetooth 4.0**: Vgate vLinker MC+ (BLE-Version) oder OBDLink CX | Der ESP32-S3 kann kein Bluetooth Classic, deshalb BLE. Keine billigen "v2.1"-Klone |
| Strom | Kfz-USB-Ladegerät + USB-C-Datenkabel (liegt dem Freenove bei) | Display geht mit der Zündung aus |

## Bauen und flashen

1. [VS Code](https://code.visualstudio.com/) + Erweiterung **PlatformIO IDE** installieren.
2. Ordner `car-display` in VS Code öffnen.
3. `include/config.h` anpassen: Kraftstoff. Der Adapter wird automatisch gefunden; falls nicht, Name (`ELM_BT_NAME`) oder MAC (`ELM_BT_MAC`) eintragen. Gefundene Geräte stehen im seriellen Monitor.
4. Board per USB-C anschließen, unten auf **Upload** (→) klicken.
   Kommandozeile: `pio run -e freenove -t upload`. Klappt der Upload nicht: BOOT gedrückt halten, kurz RESET drücken, BOOT loslassen, Ausgabe ansehen mit `pio device monitor`.

**Ohne Auto testen:** `pio run -e simulator -t upload` erzeugt eine simulierte Fahrt.

## Seiten

Tippen aufs Display (oder BOOT-Taste) blättert weiter, **2 s lang drücken setzt die Fahrtdaten zurück**.

1. **Übersicht** – 9 Kacheln: Tempo, Drehzahl, Kühlmittel, Bordspannung, Ansaugluft, Motorlast, Verbrauch (l/100 km in Fahrt, l/h im Stand), Gaspedal, Tank
2. **Temperaturen** – Verlauf Kühlmittel, Ansaugluft, Öl (letzte 5 min)
3. **Drehzahl & Tempo** – Verlauf
4. **Spannung & Verbrauch** – Verlauf; zeigt, ob die Lichtmaschine sauber lädt
5. **Fahrt & Diagnose** – Strecke, Verbrauch, Ø-Verbrauch, Maximal-/Minimalwerte, Gemischkorrektur, Abfragen/s

Farben: Kühlmittel blau = kalt (< 70 °C), grün = warm, rot ≥ 105 °C. Spannung rot bei < 13,2 V mit laufendem Motor
(Lichtmaschine) bzw. < 12,0 V ohne Motor (Batterie) oder > 15 V. Schwellen stehen in `config.h`.
"n/v" = dein Auto liefert diesen Wert nicht.

## Wie es funktioniert

- `src/ble_serial.cpp` verbindet sich per [NimBLE](https://github.com/h2zero/NimBLE-Arduino) mit dem Adapter und sucht dessen UART-Dienst automatisch.
- `src/obd.cpp` fragt die Werte nicht-blockierend über [ELMduino](https://github.com/PowerBroker2/ELMduino) ab:
  Drehzahl, Tempo, Last und Gaspedal in jeder Runde, die übrigen Werte reihum.
  PIDs, die das Auto nicht kennt, werden nach drei Fehlversuchen übersprungen.
  Bei Verbindungsverlust verbindet es sich automatisch neu.
- Verbrauch kommt aus PID 0x5E (Fuel Rate), sonst aus dem Luftmassenstrom (MAF ÷ 14,7 ÷ 745 g/l).
- `src/ui.cpp` zeichnet mit [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI); Diagramme laufen über einen
  8-Bit-Sprite, damit nichts flackert und genug RAM für Bluetooth bleibt.

## Typische Probleme

- **"Adapter nicht gefunden"**: Im seriellen Monitor stehen alle gefundenen BLE-Geräte; Namen oder MAC in `config.h` eintragen.
  Die Handy-App des Adapters dabei schließen, ein BLE-Adapter nimmt nur eine Verbindung an.
- **"ELM327 antwortet nicht" / nur Spannung kommt**: Zündung an? Billige Klon-Adapter (v2.1) können manche
  Protokolle nicht; ein besserer Adapter hilft oft.
- **Wenige Abfragen pro Sekunde**: normal sind 5–15/s, Klone sind langsamer.
