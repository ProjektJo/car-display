# Car-Display Firmware v2

Display im Auto: Ein ESP32-S3 liest über einen BLE-OBD2-Adapter (ELM327-kompatibel) die Fahrzeugdaten und hilft vor allem beim spritsparenden Fahren. Spezifikation: `car-display-plan/master-prompt.md` mit `architektur.md`, `ui-entwurf.md` und der Vorschau.

**Stand: Etappe 1 (Gerüst).** Display, Touch und LVGL mit Theme, Statusleiste und Wischnavigation über die (noch leeren) Seiten, BOOT-Taste, Helligkeit per PWM und die Simulator-Datenquelle mit `CarState` und Snapshot. Bluetooth und OBD kommen in Etappe 2.

## Hardware

- Freenove ESP32-S3 Display 2,8" FNK0104B (N16R8, ILI9341, FT6336U-Touch). Alle Pins stehen in `include/board_fnk0104b.h`.
- Adapter Vgate vLinker MC+ in der BLE-Version (ab Etappe 2).

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
| Serieller Monitor | `pio device monitor` |

Klappt der Upload nicht: BOOT gedrückt halten, kurz RESET drücken, BOOT loslassen, nochmal hochladen.

Der erste Build lädt die Bibliotheken herunter (LVGL 9.2, TFT_eSPI, NimBLE, ArduinoJson, TinyGPSPlus) und dauert einige Minuten.

**Unit-Tests unter Windows** brauchen einen C++-Compiler auf dem PC (PlatformIO bringt für `native` keinen mit). Einfachster Weg: MSYS2 installieren, darin `pacman -S mingw-w64-ucrt-x86_64-gcc`, dann `C:\msys64\ucrt64\bin` zum PATH hinzufügen und VS Code neu starten. Unter macOS und Linux reicht der vorhandene `gcc`/`clang`.

**Speicher:** Nach dem Build stehen in der Ausgabe zwei Zeilen `RAM:` und `Flash:`. Bitte beide Zeilen mitschicken.

## Bedienung (Stand Etappe 1)

| Eingabe | Wirkung |
|---|---|
| Wischen nach links / rechts | nächste / vorige Seite, Endlosschleife |
| Von oben nach unten wischen (Start in den obersten 40 px) | Menü (Platzhalter) |
| Lang drücken (0,8 s) auf eine freie Fläche oder die Statusleiste | Menü (Platzhalter) |
| BOOT kurz | nächste Seite; ist ein Fenster offen, schließt es |
| BOOT lang (0,8 s) | Menü (Platzhalter) |
| BOOT sehr lang (2 s), nur Simulator | Vollgas-Sequenz beim nächsten Halt |

Fenster schließen sich nach 60 s ohne Berührung.

## Schriften

Die in LVGL eingebauten Montserrat-Schriften enthalten nur ASCII, also keine Umlaute, kein €, kein Ø. Deshalb liegen eigene Schriften in `src/ui/fonts/` (bereits erzeugt, nichts zu tun):

| Datei | Größe | Schnitt | Zeichen |
|---|---|---|---|
| `font_m12.c` | 12 px | Montserrat Medium | Latin-1, – … € ↑ ↓, aus DejaVu Σ ⁹ → ≈ ▲ ▼, Symbole (Zapfsäule, Tropfen, Thermometer, Warndreieck, Motor) |
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
│  ├─ core/                  CarState mit Mutex und Snapshot, Befehls-Queues, calcTask
│  ├─ hw/                    Display (TFT_eSPI + DMA, Hintergrundlicht), Touch FT6336U
│  ├─ obd/                   obdTask (Etappe 1: nur "keine Verbindung")
│  ├─ sim/                   simulierte Fahrt (reines C++) und ihre Task
│  ├─ ui/                    Theme, Statusleiste, Fenster, Seiten, Schriften
│  └─ util/                  Zahlenformat, BOOT-Taste, Wischgesten (reines C++)
├─ test/                     Unit-Tests für `pio test -e native`
└─ tools/                    Schriften erzeugen
```

## Fehlersuche

- **Bild bleibt schwarz oder zeigt Streifen:** In `include/config.h` `DISPLAY_USE_DMA` auf `0` setzen. Dann zeichnet die Firmware ohne DMA wie die alte Firmware.
- **Wischen geht in die falsche Richtung oder Tippen trifft daneben:** Beim Tippen schreibt der serielle Monitor `Touch: roh … -> x …, y …`. Oben links sollte x und y nahe 0 sein, unten rechts x nahe 319 und y nahe 239. Stimmt das nicht, in `include/board_fnk0104b.h` `BOARD_TOUCH_SWAP_XY`, `BOARD_TOUCH_INVERT_X` und `BOARD_TOUCH_INVERT_Y` anpassen und die Monitor-Zeilen mitschicken.
