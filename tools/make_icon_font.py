#!/usr/bin/env python3
"""Erzeugt cd_icons.ttf mit eigenen Symbolen, die in keiner freien Schrift stecken.

U+E000  Motorsymbol (Motorkontrollleuchte, MIL) als Silhouette aus Rechtecken.

Aufruf: python3 make_icon_font.py <ausgabe.ttf>   (braucht: pip install fonttools)
"""
import sys
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

# Rechtecke (x0, y0, x1, y1) in Font-Einheiten (1000 je em), y nach oben
ENGINE = [
    (60, 200, 130, 520),    # Lüfter links
    (130, 320, 230, 400),   # Verbindung Lüfter
    (230, 120, 780, 560),   # Motorblock
    (420, 560, 580, 640),   # Ansaugstutzen
    (330, 640, 670, 710),   # Deckel
    (780, 280, 860, 400),   # Verbindung Auspuff
    (860, 180, 930, 500),   # Auspuff rechts
]


def rects_glyph(rects):
    pen = TTGlyphPen(None)
    for x0, y0, x1, y1 in rects:  # TrueType: Außenkontur im Uhrzeigersinn
        pen.moveTo((x0, y0))
        pen.lineTo((x0, y1))
        pen.lineTo((x1, y1))
        pen.lineTo((x1, y0))
        pen.closePath()
    return pen.glyph()


def main(out):
    fb = FontBuilder(1000, isTTF=True)
    names = [".notdef", "engine"]
    fb.setupGlyphOrder(names)
    fb.setupCharacterMap({0xE000: "engine"})
    empty = TTGlyphPen(None).glyph()
    fb.setupGlyf({".notdef": empty, "engine": rects_glyph(ENGINE)})
    fb.setupHorizontalMetrics({".notdef": (500, 0), "engine": (1000, 60)})
    fb.setupHorizontalHeader(ascent=800, descent=-200)
    fb.setupNameTable({"familyName": "CarDisplayIcons", "styleName": "Regular"})
    fb.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200)
    fb.setupPost()
    fb.save(out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "cd_icons.ttf")
