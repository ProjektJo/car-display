# Car-Display Firmware v2

Display im Auto: Ein ESP32-S3 liest über einen BLE-OBD2-Adapter (ELM327-kompatibel) die Fahrzeugdaten und hilft vor allem beim spritsparenden Fahren. Spezifikation: `car-display-plan/master-prompt.md` mit `architektur.md`, `ui-entwurf.md` und der Vorschau.

**Stand: Etappe 6 (Sport und Sprint).** Aus Etappe 1 bis 3: Display, Touch, LVGL mit Theme, Statusleiste, Wischnavigation, BOOT-Taste, Helligkeit, Simulator, Bluetooth-Verbindung, ELM327-Client, PID-Scheduler, Startbildschirm, Diagnose, Verbrauch, Mittelwerte, Tank und Reichweite, Kalibrierung, Fahrten, Speichern im Flash und Fahrzeugprofile. Etappe 4: Eco-Seite mit Momentanverbrauch, Eco-Kurve, Gang mit Schaltpfeil, Spartipps, Eco-Score, Schub gespart und Gebremst, Gänge lernen, Fahrzeug-Prüfung und Start-Karte. Etappe 5: Übersicht mit belegbaren Kacheln, Großanzeige, Fahrt & Tank, Tank-Fenster mit automatischer Tankerkennung, Diagramme. Etappe 6: Sport-Seite mit Drehzahlbogen und Live-Diagramm, Sprint-Seite mit 0–50, 0–100, 80–120 und Auto-Sprint. Einzelheiten: `docs/etappe4.md` bis `docs/etappe6.md`.

## Hardware

- Freenove ESP32-S3 Display 2,8" FNK0104B (N16R8, ILI9341, FT6336U-Touch). Alle Pins stehen in `include/board_fnk0104b.h`.
- Adapter Vgate vLinker MC+ in der BLE-Version ("iOS" bzw. "BT 4.0"). Die Firmware sucht ihn automatisch: Sie nimmt den ersten Bluetooth-Namen, der nach OBD-Adapter aussieht (OBD, VLINK, VGATE, ELM, ICAR, V-LINK, KONNWEI). Für einen bestimmten Adapter in `include/config.h` `BLE_ADAPTER_NAME` oder `BLE_ADAPTER_MAC` eintragen.

## Bauen und flashen

1. VS Code mit der Erweiterung **PlatformIO IDE** installieren und den Ordner `car-display-v2` öffnen.
2. Board per USB-C anschließen.
3. In einem PlatformIO-Terminal:

| Zweck | Befehl |
|---|---|
| Ohne Auto am Schreibtisch (simulierte Fahrt) | `pio run -e simulator -t upload` |
| Für das Auto | `pio run -e freenove -t upload` |
| Nur kompilieren | `pio run -e freenove` und `pio run -e simulator` |
| Unit-Tests auf dem PC | `pio test -e native` |
| Unit-Tests auf dem Board (ohne Compiler auf dem PC) | `pio test -e board_test` |
| Bildschirmfoto vom Board (Fehlersuche) | `python tools/screenshot.py COM7 bild.png` |
| Serieller Monitor | `pio device monitor` |

Klappt der Upload nicht: BOOT gedrückt halten, kurz RESET drücken, BOOT loslassen, nochmal hochladen.

Der erste Build lädt die Bibliotheken herunter (LVGL 9.2, TFT_eSPI, NimBLE, ArduinoJson, TinyGPSPlus) und dauert einige Minuten.

**Unit-Tests unter Windows** brauchen einen C++-Compiler auf dem PC (PlatformIO bringt für `native` keinen mit). Einfachster Weg: MSYS2 installieren, darin `pacman -S mingw-w64-ucrt-x86_64-gcc`, dann `C:\msys64\ucrt64\bin` zum PATH hinzufügen und VS Code neu starten. Unter macOS und Linux reicht der vorhandene `gcc`/`clang`.

**Speicher:** Nach dem Build stehen in der Ausgabe zwei Zeilen `RAM:` und `Flash:`. Bitte beide Zeilen mitschicken.

## Verbindung

Nach dem Einschalten zeigt der Startbildschirm die Schritte: "Suche Adapter …", "Verbunden mit …", "Protokoll: …", "Fahrzeug: …". Bei einem Fehler steht dort ein Satz mit der Lösung und die Zeit bis zum nächsten Versuch (Pausen 1, 2, 5, 10 s, danach alle 10 s). Tippen blendet den Startbildschirm aus, lang drücken öffnet das Menü.

Reißt die Verbindung später ab, bleibt die Seite stehen. Der Punkt links in der Statusleiste wird grau, die Werte zeigen "–", und die Firmware verbindet sich von selbst neu.

Menü → **Diagnose** zeigt Adapter, Protokoll, VIN (oder "nicht geliefert"), Abfragen pro Sekunde, unterstützte PIDs (mit den fehlenden wichtigen wie Tank oder Gaspedal), die Verbrauchsquelle und die Fahrzeugart.

Der serielle Monitor (`pio device monitor`) schreibt beim Verbinden mit:

```
BLE: Suche Adapter …
  gefunden: 00:10:cc:4f:36:03  vLinker MC-IOS
BLE-UART: Dienst fff0, RX fff1, TX fff2
ELM: ELM327 v2.2
OBD: Protokoll 5 (ISO 14230-4 KWP)
OBD: 11 unterstützte PIDs: 01 03 04 05 06 07 0B 0C 0D 0F 11 20
OBD: läuft, 11 Werte im Plan
```

Bitte die Zeile mit den unterstützten PIDs nach der ersten Fahrt aufheben: Sie zeigt, ob der Modus Tankfüllstand (2F) und Gaspedal (49) liefert.

## Bedienung (Stand Etappe 6)

| Eingabe | Wirkung |
|---|---|
| Wischen nach links / rechts | nächste / vorige Seite, Endlosschleife |
| Von oben nach unten wischen (Start in den obersten 40 px) | Menü |
| Lang drücken (0,8 s) auf eine freie Fläche oder die Statusleiste | Menü (bis Etappe 7: nur Diagnose) |
| BOOT kurz | nächste Seite; ist ein Fenster offen, schließt es; blendet den Startbildschirm aus |
| BOOT lang (0,8 s) | Menü |
| BOOT sehr lang (2 s), nur Simulator | Vollgas-Sequenz beim nächsten Halt |

Fenster schließen sich nach 60 s ohne Berührung.

## Fahrzeugprofile

Nach dem Verbinden sucht die Firmware das Profil zum Auto: erst über die VIN, sonst über die Liste der unterstützten PIDs und das Protokoll. Passt genau eins, wird es geladen. Passt keins oder passen mehrere, fragt das Display "Welches Fahrzeug?". Beim allerersten Start öffnet sich gleich der Assistent "Neues Fahrzeug" (Name, Kraftstoff, Hubraum, Tankgröße), vorausgefüllt mit dem Modus. Menü → Diagnose → "Fahrzeug" öffnet die Auswahl jederzeit.

Bis das Auto feststeht, rechnet die Firmware nichts und zählt keine Kilometer. Bis zu 8 Profile, jedes mit eigenen Mittelwerten, Tank, Fahrten und Tankfüllungen.

Ablage im Flash (LittleFS): `/profiles/p<Nr>.json` und `/data/p<Nr>_…`. Die Umgebung `simulator` legt alles unter `/sim` ab und stört die echten Profile nicht.

## Schriften

Die in LVGL eingebauten Montserrat-Schriften enthalten nur ASCII, also keine Umlaute, kein €, kein Ø. Deshalb liegen eigene Schriften in `src/ui/fonts/` (bereits erzeugt, nichts zu tun):

| Datei | Größe | Schnitt | Zeichen |
|---|---|---|---|
| `font_m12.c` | 12 px | Montserrat Medium | Latin-1, – … € ↑ ↓, aus DejaVu Σ ⁹ → ≈ ▲ ▼, Symbole (Zapfsäule, Tropfen, Thermometer, Warndreieck, Motor, Rücktaste) |
| `font_m14.c` | 14 px | Montserrat Medium | wie 12 px |
| `font_m20.c` | 20 px | Montserrat SemiBold | wie 12 px ohne Motorsymbol |
| `font_m28.c` | 28 px | Montserrat SemiBold | Latin-1 und Sonderzeichen, keine Symbole |
| `font_m48.c` | 48 px | Montserrat SemiBold | ASCII, °, – |
| `font_d72.c` | 72 px | Montserrat SemiBold | nur `0-9 , . - / :`, Leerzeichen und – (für "kein Wert") |

Neu erzeugen (nur nötig, wenn Zeichen fehlen): `bash tools/make_fonts.sh`. Das Skript holt die Quellschriften per npm (`@fontsource/montserrat@5.3.0`, `dejavu-fonts-ttf@2.37.3`, `@fortawesome/fontawesome-free@7.3.1`) und baut das Motorsymbol mit `tools/make_icon_font.py`. Voraussetzungen: `npm i -g lv_font_conv@1.5.3` und `pip install fonttools brotli`.

Der genaue Befehl für die 72-px-Ziffernschrift lautet:

```
lv_font_conv --no-compress --no-prefilter --bpp 4 --size 72 --format lvgl --lv-font-name font_d72 \
  -o src/ui/fonts/font_d72.c --font mont600.ttf -r 0x20,0x2C-0x3A,0x2013
```

(`mont600.ttf` ist `montserrat-latin-600-normal.woff2` aus `@fontsource/montserrat`, mit fonttools nach TTF gewandelt; das Skript macht das selbst.)

## Ordner

```
car-display-v2/
├─ platformio.ini            Umgebungen freenove, simulator, native
├─ partitions.csv            App 6 MB, LittleFS 4 MB, NVS 92 kB
├─ include/
│  ├─ board_fnk0104b.h       alle Pins und das TFT_eSPI-Setup
│  ├─ config.h               Schwellen und Zeiten mit Fundstelle
│  └─ lv_conf.h              LVGL 9.2
├─ src/
│  ├─ main.cpp               startet die Tasks
│  ├─ calc/                  Verbrauch, Mittelwerte, Fahrt, Tank, Reichweite (reines C++)
│  ├─ core/                  CarState mit Mutex und Snapshot, Befehls-Queues, calcTask, Profil
│  ├─ hw/                    Display (TFT_eSPI + DMA, Hintergrundlicht), Touch FT6336U
│  ├─ obd/                   BLE-Link, ELM327-Client, Antwort-Parser, PID-Scheduler, obdTask
│  ├─ sim/                   simulierte Fahrt (reines C++) und ihre Task
│  ├─ storage/               LittleFS und NVS, Profile (JSON), Profil erkennen, storageTask
│  ├─ ui/                    Theme, Statusleiste, Startbildschirm, Menü, Fenster, Fahrzeugauswahl, Seiten, Schriften
│  └─ util/                  Zahlenformat, BOOT-Taste, Wischgesten, Verbindungstexte (reines C++)
├─ test/                     Unit-Tests für `pio test -e native`
└─ tools/                    Schriften erzeugen
```

## Fehlersuche

- **"Kein Adapter gefunden":** Zündung an (der Adapter braucht Strom aus der OBD-Buchse)? Eine Handy-App, die mit dem Adapter verbunden ist, schließen; der Adapter nimmt nur eine Verbindung an. Im seriellen Monitor stehen unter "gefunden:" alle Bluetooth-Geräte in der Nähe. Steht der Adapter dort mit einem anderen Namen, diesen Namen in `include/config.h` bei `BLE_ADAPTER_NAME` eintragen.
- **"Auto antwortet nicht":** Zündung an? Die Firmware sucht das Protokoll automatisch, beim Modus mit KWP dauert das bis zu 15 s.
- **Bild bleibt schwarz:** Gleich nach dem Start zeigt das Display "Car-Display startet" (noch ohne LVGL). Fehlt auch diese Zeile und bleibt das Licht aus, liegt es an Display, Licht oder Board-Einstellung. Steht die Zeile da, aber es geht nicht weiter, hängt es später: Der serielle Monitor (`pio device monitor`, dann RESET drücken) zeigt mit den Zeilen `Start: …`, bis wohin die Firmware kommt, und in der ersten Zeile den Grund des letzten Neustarts (z. B. ABSTURZ oder WATCHDOG).
- **DMA:** `DISPLAY_USE_DMA` in `include/config.h` steht auf `0` (sicherer Weg wie in der alten Firmware), bis DMA auf dem Board bestätigt ist. Mit `1` lässt es sich später erneut testen; zeigt es dann Streifen oder Schwarz, zurück auf `0`.
- **Wischen geht in die falsche Richtung oder Tippen trifft daneben:** Beim Tippen schreibt der serielle Monitor `Touch: roh … -> x …, y …`. Oben links sollte x und y nahe 0 sein, unten rechts x nahe 319 und y nahe 239. Stimmt das nicht, in `include/board_fnk0104b.h` `BOARD_TOUCH_SWAP_XY`, `BOARD_TOUCH_INVERT_X` und `BOARD_TOUCH_INVERT_Y` anpassen und die Monitor-Zeilen mitschicken.
