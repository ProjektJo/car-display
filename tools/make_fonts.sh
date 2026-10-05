#!/usr/bin/env bash
# Erzeugt die Schriften in src/ui/fonts/ (Montserrat mit Umlauten + Symbole).
#
# Warum eigene Schriften: Die in LVGL eingebauten Montserrat-Schriften enthalten nur ASCII,
# also keine Umlaute, kein €, kein Ø. Deshalb werden sie hier mit lv_font_conv neu erzeugt.
#
# Voraussetzungen: Node.js (npm) und Python 3 mit fonttools:
#   npm i -g lv_font_conv@1.5.3
#   pip install fonttools brotli
# Aufruf aus dem Projektordner:  bash tools/make_fonts.sh
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=src/ui/fonts
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# Quellschriften aus npm holen (alle frei: OFL bzw. Bitstream-Vera-Lizenz bzw. CC BY 4.0/OFL)
( cd "$WORK" && npm pack --silent @fontsource/montserrat@5.3.0 dejavu-fonts-ttf@2.37.3 @fortawesome/fontawesome-free@7.3.1 >/dev/null \
  && for f in *.tgz; do mkdir -p "${f%.tgz}" && tar xzf "$f" -C "${f%.tgz}"; done )
MS=$(ls -d "$WORK"/fontsource-montserrat-*/package/files)
DJ=$(ls -d "$WORK"/dejavu-fonts-ttf-*/package/ttf)
FA=$(ls -d "$WORK"/fortawesome-fontawesome-free-*/package/webfonts)

# woff2 -> ttf
python3 - "$MS" "$FA" "$WORK" <<'PY'
import sys
from fontTools.ttLib import TTFont
ms, fa, work = sys.argv[1:4]
for src, dst in [(f"{ms}/montserrat-latin-500-normal.woff2", "mont500.ttf"),
                 (f"{ms}/montserrat-latin-600-normal.woff2", "mont600.ttf"),
                 (f"{fa}/fa-solid-900.woff2", "fa-solid.ttf")]:
    f = TTFont(src); f.flavor = None; f.save(f"{work}/{dst}")
PY
python3 tools/make_icon_font.py "$WORK/cd_icons.ttf"

# Zeichenvorrat
TEXT="0x20-0x7E,0xA0-0xFF,0x2013,0x2014,0x2026,0x20AC,0x2191,0x2193"   # Latin-1 (ä ö ü ß ° · Ø ²), – — … € ↑ ↓
EXTRA="0x03A3,0x2079,0x2192,0x2248,0x25B2,0x25BC"                      # Σ ⁹ → ≈ ▲ ▼ (fehlen in Montserrat, aus DejaVu Sans)
# FontAwesome: Zapfsäule, Tropfen, Thermometer, Warndreieck sowie die LVGL-Standardsymbole OK, X, ◀ ▶ ▲ ▼, Zahnrad, Löschtaste
ICONS="0xF52F,0xF043,0xF2C9,0xF071,0xF00C,0xF00D,0xF053,0xF054,0xF077,0xF078,0xF013,0xF55A"
ENGINE="0xE000"                                                          # Motorsymbol (MIL), eigene Datei
DIGITS="0x20,0x2C-0x3A,0x2013"                                            # Leerzeichen , - . / 0-9 : und – für "kein Wert"

conv() { # name size weight extra-args...
  local name=$1 size=$2 weight=$3; shift 3
  local dj="$DJ/DejaVuSans.ttf"; [ "$weight" = 600 ] && dj="$DJ/DejaVuSans-Bold.ttf"
  lv_font_conv --no-compress --no-prefilter --bpp 4 --size "$size" --format lvgl --lv-font-name "$name" \
    -o "$OUT/$name.c" "$@"
  sed -i "s|$WORK|<npm-pakete>|g" "$OUT/$name.c"   # keine Temp-Pfade im Dateikopf
}

conv font_m12 12 500 --font "$WORK/mont500.ttf" -r "$TEXT" --font "$DJ/DejaVuSans.ttf" -r "$EXTRA" \
  --font "$WORK/fa-solid.ttf" -r "$ICONS" --font "$WORK/cd_icons.ttf" -r "$ENGINE"
conv font_m14 14 500 --font "$WORK/mont500.ttf" -r "$TEXT" --font "$DJ/DejaVuSans.ttf" -r "$EXTRA" \
  --font "$WORK/fa-solid.ttf" -r "$ICONS" --font "$WORK/cd_icons.ttf" -r "$ENGINE"
conv font_m20 20 600 --font "$WORK/mont600.ttf" -r "$TEXT" --font "$DJ/DejaVuSans-Bold.ttf" -r "$EXTRA" \
  --font "$WORK/fa-solid.ttf" -r "$ICONS"
conv font_m28 28 600 --font "$WORK/mont600.ttf" -r "$TEXT" --font "$DJ/DejaVuSans-Bold.ttf" -r "$EXTRA"
conv font_m48 48 600 --font "$WORK/mont600.ttf" -r "0x20-0x7E,0xB0,0x2013"
conv font_d72 72 600 --font "$WORK/mont600.ttf" -r "$DIGITS"
echo "Schriften erzeugt in $OUT"
