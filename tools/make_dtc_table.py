#!/usr/bin/env python3
"""Erzeugt src/obd/dtc_table.cpp aus data/dtc_de.csv (deutsche Klartexte der Fehlercodes, A11).

Aufruf aus dem Projektordner:  python tools/make_dtc_table.py
"""
import os
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "data", "dtc_de.csv")
OUT = os.path.join(ROOT, "src", "obd", "dtc_table.cpp")
LETTERS = {"P": 0, "C": 1, "B": 2, "U": 3}


def code_value(code):
    """'P0171' -> 0x0171 (Bits 15-14 Buchstabe, dann vier Ziffern wie im OBD-Protokoll)."""
    if len(code) != 5 or code[0] not in LETTERS:
        raise ValueError(code)
    first = int(code[1], 16)
    if first > 3:
        raise ValueError(code)
    return (LETTERS[code[0]] << 14) | (first << 12) | int(code[2:], 16)


def c_string(text):
    """C-Zeichenkette mit UTF-8 als Oktal-Escapes, damit der Compiler keine Kodierung raten muss."""
    out = []
    for b in text.encode("utf-8"):
        if b < 0x80 and chr(b) not in '"\\':
            out.append(chr(b))
        else:
            out.append("\\%03o" % b)
    return '"' + "".join(out) + '"'


def main():
    entries = {}
    with open(SRC, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            code, _, text = line.partition(";")
            code, text = code.strip().upper(), text.strip()
            if not text:
                sys.exit(f"{SRC}:{n}: Text fehlt")
            v = code_value(code)
            if v in entries:
                sys.exit(f"{SRC}:{n}: {code} doppelt")
            entries[v] = (code, text)
    lines = [
        "// ERZEUGT von tools/make_dtc_table.py aus data/dtc_de.csv. Nicht von Hand ändern.",
        '#include "dtc.h"',
        "",
        "namespace dtc {",
        "",
        "struct Entry {",
        "  Code code;",
        "  const char* text;",
        "};",
        "",
        "extern const Entry TABLE[];",
        "extern const int TABLE_SIZE;",
        "",
        "const Entry TABLE[] = {",
    ]
    for v in sorted(entries):
        code, text = entries[v]
        lines.append(f"    {{0x{v:04X}, {c_string(text)}}},  // {code}")
    lines += ["};", "", "const int TABLE_SIZE = sizeof(TABLE) / sizeof(TABLE[0]);", "", "}  // namespace dtc", ""]
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    print(f"{len(entries)} Codes -> {OUT}")


if __name__ == "__main__":
    main()
