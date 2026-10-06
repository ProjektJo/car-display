// ERZEUGT von tools/make_dtc_table.py aus data/dtc_de.csv. Nicht von Hand ändern.
#include "dtc.h"

namespace dtc {

struct Entry {
  Code code;
  const char* text;
};

extern const Entry TABLE[];
extern const int TABLE_SIZE;

const Entry TABLE[] = {
    {0x0010, "Nockenwellenversteller Einlass Bank 1: Stromkreis"},  // P0010
    {0x0011, "Nockenwelle Einlass Bank 1: zu fr\303\274h"},  // P0011
    {0x0012, "Nockenwelle Einlass Bank 1: zu sp\303\244t"},  // P0012
    {0x0013, "Nockenwellenversteller Auslass Bank 1: Stromkreis"},  // P0013
    {0x0014, "Nockenwelle Auslass Bank 1: zu fr\303\274h"},  // P0014
    {0x0015, "Nockenwelle Auslass Bank 1: zu sp\303\244t"},  // P0015
    {0x0016, "Kurbelwelle/Nockenwelle Einlass Bank 1: Zuordnung"},  // P0016
    {0x0017, "Kurbelwelle/Nockenwelle Auslass Bank 1: Zuordnung"},  // P0017
    {0x0020, "Nockenwellenversteller Einlass Bank 2: Stromkreis"},  // P0020
    {0x0021, "Nockenwelle Einlass Bank 2: zu fr\303\274h"},  // P0021
    {0x0022, "Nockenwelle Einlass Bank 2: zu sp\303\244t"},  // P0022
    {0x0030, "Lambdasonde 1 Bank 1, Heizung: Stromkreis"},  // P0030
    {0x0031, "Lambdasonde 1 Bank 1, Heizung: Spannung zu niedrig"},  // P0031
    {0x0032, "Lambdasonde 1 Bank 1, Heizung: Spannung zu hoch"},  // P0032
    {0x0036, "Lambdasonde 2 Bank 1, Heizung: Stromkreis"},  // P0036
    {0x0037, "Lambdasonde 2 Bank 1, Heizung: Spannung zu niedrig"},  // P0037
    {0x0038, "Lambdasonde 2 Bank 1, Heizung: Spannung zu hoch"},  // P0038
    {0x0040, "Lambdasonden-Signale vertauscht Bank 1/2"},  // P0040
    {0x0050, "Lambdasonde 1 Bank 2, Heizung: Stromkreis"},  // P0050
    {0x0051, "Lambdasonde 1 Bank 2, Heizung: Spannung zu niedrig"},  // P0051
    {0x0052, "Lambdasonde 1 Bank 2, Heizung: Spannung zu hoch"},  // P0052
    {0x0056, "Lambdasonde 2 Bank 2, Heizung: Stromkreis"},  // P0056
    {0x0057, "Lambdasonde 2 Bank 2, Heizung: Spannung zu niedrig"},  // P0057
    {0x0058, "Lambdasonde 2 Bank 2, Heizung: Spannung zu hoch"},  // P0058
    {0x0068, "Saugrohrdruck/Luftmasse passt nicht zur Drosselklappe"},  // P0068
    {0x0069, "Saugrohrdruck passt nicht zum Luftdruck"},  // P0069
    {0x0070, "Au\303\237entemperaturf\303\274hler: Stromkreis"},  // P0070
    {0x0071, "Au\303\237entemperaturf\303\274hler: Messbereich/Funktion"},  // P0071
    {0x0072, "Au\303\237entemperaturf\303\274hler: Signal zu niedrig"},  // P0072
    {0x0073, "Au\303\237entemperaturf\303\274hler: Signal zu hoch"},  // P0073
    {0x0087, "Kraftstoffdruck zu niedrig"},  // P0087
    {0x0088, "Kraftstoffdruck zu hoch"},  // P0088
    {0x0089, "Kraftstoffdruckregler: Funktion"},  // P0089
    {0x0090, "Kraftstoffdruckregler: Stromkreis"},  // P0090
    {0x0093, "Kraftstoffsystem: gro\303\237es Leck"},  // P0093
    {0x0100, "Luftmassenmesser: Stromkreis"},  // P0100
    {0x0101, "Luftmassenmesser: Messbereich/Funktion"},  // P0101
    {0x0102, "Luftmassenmesser: Signal zu niedrig"},  // P0102
    {0x0103, "Luftmassenmesser: Signal zu hoch"},  // P0103
    {0x0104, "Luftmassenmesser: Signal unterbrochen"},  // P0104
    {0x0105, "Saugrohrdruckf\303\274hler: Stromkreis"},  // P0105
    {0x0106, "Saugrohrdruckf\303\274hler: Messbereich/Funktion"},  // P0106
    {0x0107, "Saugrohrdruckf\303\274hler: Signal zu niedrig"},  // P0107
    {0x0108, "Saugrohrdruckf\303\274hler: Signal zu hoch"},  // P0108
    {0x0109, "Saugrohrdruckf\303\274hler: Signal unterbrochen"},  // P0109
    {0x0110, "Ansauglufttemperaturf\303\274hler: Stromkreis"},  // P0110
    {0x0111, "Ansauglufttemperaturf\303\274hler: Messbereich/Funktion"},  // P0111
    {0x0112, "Ansauglufttemperaturf\303\274hler: Signal zu niedrig"},  // P0112
    {0x0113, "Ansauglufttemperaturf\303\274hler: Signal zu hoch"},  // P0113
    {0x0114, "Ansauglufttemperaturf\303\274hler: Signal unterbrochen"},  // P0114
    {0x0115, "K\303\274hlmitteltemperaturf\303\274hler: Stromkreis"},  // P0115
    {0x0116, "K\303\274hlmitteltemperaturf\303\274hler: Messbereich/Funktion"},  // P0116
    {0x0117, "K\303\274hlmitteltemperaturf\303\274hler: Signal zu niedrig"},  // P0117
    {0x0118, "K\303\274hlmitteltemperaturf\303\274hler: Signal zu hoch"},  // P0118
    {0x0119, "K\303\274hlmitteltemperaturf\303\274hler: Signal unterbrochen"},  // P0119
    {0x0120, "Drosselklappen-/Pedalgeber A: Stromkreis"},  // P0120
    {0x0121, "Drosselklappen-/Pedalgeber A: Messbereich/Funktion"},  // P0121
    {0x0122, "Drosselklappen-/Pedalgeber A: Signal zu niedrig"},  // P0122
    {0x0123, "Drosselklappen-/Pedalgeber A: Signal zu hoch"},  // P0123
    {0x0124, "Drosselklappen-/Pedalgeber A: Signal unterbrochen"},  // P0124
    {0x0125, "K\303\274hlmittel zu kalt f\303\274r Lambdaregelung"},  // P0125
    {0x0126, "K\303\274hlmittel zu kalt f\303\274r stabilen Betrieb"},  // P0126
    {0x0128, "Thermostat: K\303\274hlmittel unter Solltemperatur"},  // P0128
    {0x0130, "Lambdasonde 1 Bank 1: Stromkreis"},  // P0130
    {0x0131, "Lambdasonde 1 Bank 1: Spannung zu niedrig"},  // P0131
    {0x0132, "Lambdasonde 1 Bank 1: Spannung zu hoch"},  // P0132
    {0x0133, "Lambdasonde 1 Bank 1: Reaktion zu langsam"},  // P0133
    {0x0134, "Lambdasonde 1 Bank 1: keine Aktivit\303\244t"},  // P0134
    {0x0135, "Lambdasonde 1 Bank 1, Heizung: Fehlfunktion"},  // P0135
    {0x0136, "Lambdasonde 2 Bank 1: Stromkreis"},  // P0136
    {0x0137, "Lambdasonde 2 Bank 1: Spannung zu niedrig"},  // P0137
    {0x0138, "Lambdasonde 2 Bank 1: Spannung zu hoch"},  // P0138
    {0x0139, "Lambdasonde 2 Bank 1: Reaktion zu langsam"},  // P0139
    {0x0140, "Lambdasonde 2 Bank 1: keine Aktivit\303\244t"},  // P0140
    {0x0141, "Lambdasonde 2 Bank 1, Heizung: Fehlfunktion"},  // P0141
    {0x0150, "Lambdasonde 1 Bank 2: Stromkreis"},  // P0150
    {0x0151, "Lambdasonde 1 Bank 2: Spannung zu niedrig"},  // P0151
    {0x0152, "Lambdasonde 1 Bank 2: Spannung zu hoch"},  // P0152
    {0x0153, "Lambdasonde 1 Bank 2: Reaktion zu langsam"},  // P0153
    {0x0154, "Lambdasonde 1 Bank 2: keine Aktivit\303\244t"},  // P0154
    {0x0155, "Lambdasonde 1 Bank 2, Heizung: Fehlfunktion"},  // P0155
    {0x0156, "Lambdasonde 2 Bank 2: Stromkreis"},  // P0156
    {0x0157, "Lambdasonde 2 Bank 2: Spannung zu niedrig"},  // P0157
    {0x0158, "Lambdasonde 2 Bank 2: Spannung zu hoch"},  // P0158
    {0x0159, "Lambdasonde 2 Bank 2: Reaktion zu langsam"},  // P0159
    {0x0160, "Lambdasonde 2 Bank 2: keine Aktivit\303\244t"},  // P0160
    {0x0161, "Lambdasonde 2 Bank 2, Heizung: Fehlfunktion"},  // P0161
    {0x0170, "Gemischregelung Bank 1: Fehlfunktion"},  // P0170
    {0x0171, "Gemisch zu mager (Bank 1)"},  // P0171
    {0x0172, "Gemisch zu fett (Bank 1)"},  // P0172
    {0x0173, "Gemischregelung Bank 2: Fehlfunktion"},  // P0173
    {0x0174, "Gemisch zu mager (Bank 2)"},  // P0174
    {0x0175, "Gemisch zu fett (Bank 2)"},  // P0175
    {0x0180, "Kraftstofftemperaturf\303\274hler A: Stromkreis"},  // P0180
    {0x0181, "Kraftstofftemperaturf\303\274hler A: Messbereich/Funktion"},  // P0181
    {0x0182, "Kraftstofftemperaturf\303\274hler A: Signal zu niedrig"},  // P0182
    {0x0183, "Kraftstofftemperaturf\303\274hler A: Signal zu hoch"},  // P0183
    {0x0190, "Kraftstoffdruckf\303\274hler (Rail): Stromkreis"},  // P0190
    {0x0191, "Kraftstoffdruckf\303\274hler (Rail): Messbereich/Funktion"},  // P0191
    {0x0192, "Kraftstoffdruckf\303\274hler (Rail): Signal zu niedrig"},  // P0192
    {0x0193, "Kraftstoffdruckf\303\274hler (Rail): Signal zu hoch"},  // P0193
    {0x0194, "Kraftstoffdruckf\303\274hler (Rail): Signal unterbrochen"},  // P0194
    {0x0195, "Motor\303\266ltemperaturf\303\274hler: Stromkreis"},  // P0195
    {0x0196, "Motor\303\266ltemperaturf\303\274hler: Messbereich/Funktion"},  // P0196
    {0x0197, "Motor\303\266ltemperaturf\303\274hler: Signal zu niedrig"},  // P0197
    {0x0198, "Motor\303\266ltemperaturf\303\274hler: Signal zu hoch"},  // P0198
    {0x0200, "Einspritzventile: Stromkreis"},  // P0200
    {0x0201, "Einspritzventil Zylinder 1: Stromkreis"},  // P0201
    {0x0202, "Einspritzventil Zylinder 2: Stromkreis"},  // P0202
    {0x0203, "Einspritzventil Zylinder 3: Stromkreis"},  // P0203
    {0x0204, "Einspritzventil Zylinder 4: Stromkreis"},  // P0204
    {0x0205, "Einspritzventil Zylinder 5: Stromkreis"},  // P0205
    {0x0206, "Einspritzventil Zylinder 6: Stromkreis"},  // P0206
    {0x0207, "Einspritzventil Zylinder 7: Stromkreis"},  // P0207
    {0x0208, "Einspritzventil Zylinder 8: Stromkreis"},  // P0208
    {0x0216, "Einspritzzeitpunkt: Stromkreis"},  // P0216
    {0x0217, "Motor \303\274berhitzt"},  // P0217
    {0x0218, "Getriebe \303\274berhitzt"},  // P0218
    {0x0219, "Motor-\303\234berdrehzahl"},  // P0219
    {0x0220, "Drosselklappen-/Pedalgeber B: Stromkreis"},  // P0220
    {0x0221, "Drosselklappen-/Pedalgeber B: Messbereich/Funktion"},  // P0221
    {0x0222, "Drosselklappen-/Pedalgeber B: Signal zu niedrig"},  // P0222
    {0x0223, "Drosselklappen-/Pedalgeber B: Signal zu hoch"},  // P0223
    {0x0225, "Drosselklappen-/Pedalgeber C: Stromkreis"},  // P0225
    {0x0230, "Kraftstoffpumpe: Stromkreis Prim\303\244rseite"},  // P0230
    {0x0231, "Kraftstoffpumpe: Sekund\303\244rkreis zu niedrig"},  // P0231
    {0x0232, "Kraftstoffpumpe: Sekund\303\244rkreis zu hoch"},  // P0232
    {0x0234, "Ladedruck zu hoch (\303\234berladung)"},  // P0234
    {0x0235, "Ladedruckf\303\274hler A: Stromkreis"},  // P0235
    {0x0236, "Ladedruckf\303\274hler A: Messbereich/Funktion"},  // P0236
    {0x0237, "Ladedruckf\303\274hler A: Signal zu niedrig"},  // P0237
    {0x0238, "Ladedruckf\303\274hler A: Signal zu hoch"},  // P0238
    {0x0243, "Wastegate-Magnetventil A: Stromkreis"},  // P0243
    {0x0261, "Einspritzventil Zylinder 1: Signal zu niedrig"},  // P0261
    {0x0262, "Einspritzventil Zylinder 1: Signal zu hoch"},  // P0262
    {0x0264, "Einspritzventil Zylinder 2: Signal zu niedrig"},  // P0264
    {0x0265, "Einspritzventil Zylinder 2: Signal zu hoch"},  // P0265
    {0x0267, "Einspritzventil Zylinder 3: Signal zu niedrig"},  // P0267
    {0x0268, "Einspritzventil Zylinder 3: Signal zu hoch"},  // P0268
    {0x0270, "Einspritzventil Zylinder 4: Signal zu niedrig"},  // P0270
    {0x0271, "Einspritzventil Zylinder 4: Signal zu hoch"},  // P0271
    {0x0299, "Ladedruck zu niedrig"},  // P0299
    {0x0300, "Z\303\274ndaussetzer erkannt (mehrere Zylinder)"},  // P0300
    {0x0301, "Z\303\274ndaussetzer Zylinder 1"},  // P0301
    {0x0302, "Z\303\274ndaussetzer Zylinder 2"},  // P0302
    {0x0303, "Z\303\274ndaussetzer Zylinder 3"},  // P0303
    {0x0304, "Z\303\274ndaussetzer Zylinder 4"},  // P0304
    {0x0305, "Z\303\274ndaussetzer Zylinder 5"},  // P0305
    {0x0306, "Z\303\274ndaussetzer Zylinder 6"},  // P0306
    {0x0307, "Z\303\274ndaussetzer Zylinder 7"},  // P0307
    {0x0308, "Z\303\274ndaussetzer Zylinder 8"},  // P0308
    {0x0313, "Z\303\274ndaussetzer bei niedrigem Tankinhalt"},  // P0313
    {0x0314, "Z\303\274ndaussetzer, Zylinder unbekannt"},  // P0314
    {0x0320, "Drehzahlgeber (Z\303\274ndung): Stromkreis"},  // P0320
    {0x0321, "Drehzahlgeber (Z\303\274ndung): Messbereich/Funktion"},  // P0321
    {0x0322, "Drehzahlgeber (Z\303\274ndung): kein Signal"},  // P0322
    {0x0325, "Klopfsensor 1 Bank 1: Stromkreis"},  // P0325
    {0x0326, "Klopfsensor 1 Bank 1: Messbereich/Funktion"},  // P0326
    {0x0327, "Klopfsensor 1 Bank 1: Signal zu niedrig"},  // P0327
    {0x0328, "Klopfsensor 1 Bank 1: Signal zu hoch"},  // P0328
    {0x0330, "Klopfsensor 2 Bank 2: Stromkreis"},  // P0330
    {0x0332, "Klopfsensor 2 Bank 2: Signal zu niedrig"},  // P0332
    {0x0333, "Klopfsensor 2 Bank 2: Signal zu hoch"},  // P0333
    {0x0335, "Kurbelwellensensor A: Stromkreis"},  // P0335
    {0x0336, "Kurbelwellensensor A: Messbereich/Funktion"},  // P0336
    {0x0337, "Kurbelwellensensor A: Signal zu niedrig"},  // P0337
    {0x0338, "Kurbelwellensensor A: Signal zu hoch"},  // P0338
    {0x0339, "Kurbelwellensensor A: Signal unterbrochen"},  // P0339
    {0x0340, "Nockenwellensensor A Bank 1: Stromkreis"},  // P0340
    {0x0341, "Nockenwellensensor A Bank 1: Messbereich/Funktion"},  // P0341
    {0x0342, "Nockenwellensensor A Bank 1: Signal zu niedrig"},  // P0342
    {0x0343, "Nockenwellensensor A Bank 1: Signal zu hoch"},  // P0343
    {0x0344, "Nockenwellensensor A Bank 1: Signal unterbrochen"},  // P0344
    {0x0345, "Nockenwellensensor A Bank 2: Stromkreis"},  // P0345
    {0x0350, "Z\303\274ndspule: Stromkreis"},  // P0350
    {0x0351, "Z\303\274ndspule A (Zylinder 1): Stromkreis"},  // P0351
    {0x0352, "Z\303\274ndspule B (Zylinder 2): Stromkreis"},  // P0352
    {0x0353, "Z\303\274ndspule C (Zylinder 3): Stromkreis"},  // P0353
    {0x0354, "Z\303\274ndspule D (Zylinder 4): Stromkreis"},  // P0354
    {0x0355, "Z\303\274ndspule E (Zylinder 5): Stromkreis"},  // P0355
    {0x0356, "Z\303\274ndspule F (Zylinder 6): Stromkreis"},  // P0356
    {0x0365, "Nockenwellensensor B Bank 1: Stromkreis"},  // P0365
    {0x0366, "Nockenwellensensor B Bank 1: Messbereich/Funktion"},  // P0366
    {0x0370, "Zeitgeber-Bezugssignal A: Fehlfunktion"},  // P0370
    {0x0380, "Gl\303\274hkerzen/Heizung A: Stromkreis"},  // P0380
    {0x0381, "Gl\303\274hanzeige: Stromkreis"},  // P0381
    {0x0400, "Abgasr\303\274ckf\303\274hrung: Durchfluss fehlerhaft"},  // P0400
    {0x0401, "Abgasr\303\274ckf\303\274hrung: Durchfluss zu gering"},  // P0401
    {0x0402, "Abgasr\303\274ckf\303\274hrung: Durchfluss zu hoch"},  // P0402
    {0x0403, "Abgasr\303\274ckf\303\274hrung: Stromkreis"},  // P0403
    {0x0404, "Abgasr\303\274ckf\303\274hrung: Messbereich/Funktion"},  // P0404
    {0x0405, "AGR-Ventilsensor A: Signal zu niedrig"},  // P0405
    {0x0406, "AGR-Ventilsensor A: Signal zu hoch"},  // P0406
    {0x0410, "Sekund\303\244rluftsystem: Fehlfunktion"},  // P0410
    {0x0411, "Sekund\303\244rluftsystem: falscher Durchfluss"},  // P0411
    {0x0412, "Sekund\303\244rluftventil A: Stromkreis"},  // P0412
    {0x0418, "Sekund\303\244rluftpumpenrelais A: Stromkreis"},  // P0418
    {0x0420, "Katalysator Bank 1: Wirkung zu gering"},  // P0420
    {0x0421, "Katalysator Bank 1 (Vorkat): Wirkung zu gering"},  // P0421
    {0x0430, "Katalysator Bank 2: Wirkung zu gering"},  // P0430
    {0x0431, "Katalysator Bank 2 (Vorkat): Wirkung zu gering"},  // P0431
    {0x0440, "Tankentl\303\274ftung: Fehlfunktion"},  // P0440
    {0x0441, "Tankentl\303\274ftung: falscher Sp\303\274ldurchfluss"},  // P0441
    {0x0442, "Tankentl\303\274ftung: kleines Leck"},  // P0442
    {0x0443, "Tankentl\303\274ftungsventil: Stromkreis"},  // P0443
    {0x0444, "Tankentl\303\274ftungsventil: Stromkreis offen"},  // P0444
    {0x0445, "Tankentl\303\274ftungsventil: Kurzschluss"},  // P0445
    {0x0446, "Tankentl\303\274ftung, Bel\303\274ftung: Stromkreis"},  // P0446
    {0x0449, "Tankentl\303\274ftung, Bel\303\274ftungsventil: Stromkreis"},  // P0449
    {0x0450, "Tankentl\303\274ftung, Drucksensor: Stromkreis"},  // P0450
    {0x0451, "Tankentl\303\274ftung, Drucksensor: Messbereich/Funktion"},  // P0451
    {0x0452, "Tankentl\303\274ftung, Drucksensor: Signal zu niedrig"},  // P0452
    {0x0453, "Tankentl\303\274ftung, Drucksensor: Signal zu hoch"},  // P0453
    {0x0455, "Tankentl\303\274ftung: gro\303\237es Leck (Tankdeckel?)"},  // P0455
    {0x0456, "Tankentl\303\274ftung: sehr kleines Leck"},  // P0456
    {0x0457, "Tankentl\303\274ftung: Leck (Tankdeckel lose)"},  // P0457
    {0x0460, "Tankf\303\274llstandsgeber: Stromkreis"},  // P0460
    {0x0461, "Tankf\303\274llstandsgeber: Messbereich/Funktion"},  // P0461
    {0x0462, "Tankf\303\274llstandsgeber: Signal zu niedrig"},  // P0462
    {0x0463, "Tankf\303\274llstandsgeber: Signal zu hoch"},  // P0463
    {0x0464, "Tankf\303\274llstandsgeber: Signal unterbrochen"},  // P0464
    {0x0480, "K\303\274hlerl\303\274fter 1: Stromkreis"},  // P0480
    {0x0481, "K\303\274hlerl\303\274fter 2: Stromkreis"},  // P0481
    {0x0482, "K\303\274hlerl\303\274fter 3: Stromkreis"},  // P0482
    {0x0483, "K\303\274hlerl\303\274fter: Plausibilit\303\244t"},  // P0483
    {0x0491, "Sekund\303\244rluftsystem Bank 1: Fehlfunktion"},  // P0491
    {0x0492, "Sekund\303\244rluftsystem Bank 2: Fehlfunktion"},  // P0492
    {0x0500, "Geschwindigkeitssignal: Fehlfunktion"},  // P0500
    {0x0501, "Geschwindigkeitssignal: Messbereich/Funktion"},  // P0501
    {0x0502, "Geschwindigkeitssignal: Signal zu niedrig"},  // P0502
    {0x0503, "Geschwindigkeitssignal: unterbrochen/zu hoch"},  // P0503
    {0x0505, "Leerlaufregelung: Fehlfunktion"},  // P0505
    {0x0506, "Leerlaufdrehzahl zu niedrig"},  // P0506
    {0x0507, "Leerlaufdrehzahl zu hoch"},  // P0507
    {0x0508, "Leerlaufregelung: Signal zu niedrig"},  // P0508
    {0x0509, "Leerlaufregelung: Signal zu hoch"},  // P0509
    {0x0510, "Leerlaufschalter Drosselklappe: Fehlfunktion"},  // P0510
    {0x0520, "\303\226ldruckgeber/-schalter: Stromkreis"},  // P0520
    {0x0521, "\303\226ldruckgeber/-schalter: Messbereich/Funktion"},  // P0521
    {0x0522, "\303\226ldruckgeber/-schalter: Signal zu niedrig"},  // P0522
    {0x0523, "\303\226ldruckgeber/-schalter: Signal zu hoch"},  // P0523
    {0x0530, "Klimaanlage, Drucksensor: Stromkreis"},  // P0530
    {0x0532, "Klimaanlage, Drucksensor: Signal zu niedrig"},  // P0532
    {0x0533, "Klimaanlage, Drucksensor: Signal zu hoch"},  // P0533
    {0x0560, "Bordspannung: Fehlfunktion"},  // P0560
    {0x0562, "Bordspannung zu niedrig"},  // P0562
    {0x0563, "Bordspannung zu hoch"},  // P0563
    {0x0565, "Tempomat Ein-Signal: Fehlfunktion"},  // P0565
    {0x0571, "Bremslichtschalter A: Stromkreis"},  // P0571
    {0x0600, "Datenbus (Steuerger\303\244te): Fehlfunktion"},  // P0600
    {0x0601, "Motorsteuerger\303\244t: Speicher-Pr\303\274fsumme"},  // P0601
    {0x0602, "Motorsteuerger\303\244t: Programmierfehler"},  // P0602
    {0x0603, "Motorsteuerger\303\244t: Dauerspeicher (KAM)"},  // P0603
    {0x0604, "Motorsteuerger\303\244t: Arbeitsspeicher (RAM)"},  // P0604
    {0x0605, "Motorsteuerger\303\244t: Festspeicher (ROM)"},  // P0605
    {0x0606, "Motorsteuerger\303\244t: Prozessorfehler"},  // P0606
    {0x0607, "Steuerger\303\244t: Leistungsfehler"},  // P0607
    {0x0615, "Anlasserrelais: Stromkreis"},  // P0615
    {0x0620, "Lichtmaschine: Steuerkreis"},  // P0620
    {0x0621, "Lichtmaschine, Ladekontrolle: Stromkreis"},  // P0621
    {0x0622, "Lichtmaschine, Feldsteuerung: Stromkreis"},  // P0622
    {0x0627, "Kraftstoffpumpe, Ansteuerung: Stromkreis offen"},  // P0627
    {0x0630, "Fahrgestellnummer nicht programmiert"},  // P0630
    {0x0638, "Drosselklappensteller Bank 1: Messbereich/Funktion"},  // P0638
    {0x0641, "Sensor-Referenzspannung A: Stromkreis offen"},  // P0641
    {0x0645, "Klimakompressor-Relais: Stromkreis"},  // P0645
    {0x0650, "Motorkontrollleuchte: Stromkreis"},  // P0650
    {0x0651, "Sensor-Referenzspannung B: Stromkreis offen"},  // P0651
    {0x0685, "Hauptrelais Steuerger\303\244t: Stromkreis offen"},  // P0685
    {0x0686, "Hauptrelais Steuerger\303\244t: Signal zu niedrig"},  // P0686
    {0x0700, "Getriebesteuerung: Fehler gespeichert"},  // P0700
    {0x0701, "Getriebesteuerung: Messbereich/Funktion"},  // P0701
    {0x0702, "Getriebesteuerung: elektrischer Fehler"},  // P0702
    {0x0703, "Bremsschalter B: Stromkreis"},  // P0703
    {0x0705, "Getriebe-W\303\244hlhebelsensor: Stromkreis"},  // P0705
    {0x0706, "Getriebe-W\303\244hlhebelsensor: Messbereich/Funktion"},  // P0706
    {0x0710, "Getriebe\303\266ltemperaturf\303\274hler: Stromkreis"},  // P0710
    {0x0711, "Getriebe\303\266ltemperaturf\303\274hler: Messbereich/Funktion"},  // P0711
    {0x0712, "Getriebe\303\266ltemperaturf\303\274hler: Signal zu niedrig"},  // P0712
    {0x0713, "Getriebe\303\266ltemperaturf\303\274hler: Signal zu hoch"},  // P0713
    {0x0715, "Getriebe-Eingangsdrehzahl: Stromkreis"},  // P0715
    {0x0716, "Getriebe-Eingangsdrehzahl: Messbereich/Funktion"},  // P0716
    {0x0717, "Getriebe-Eingangsdrehzahl: kein Signal"},  // P0717
    {0x0720, "Getriebe-Ausgangsdrehzahl: Stromkreis"},  // P0720
    {0x0721, "Getriebe-Ausgangsdrehzahl: Messbereich/Funktion"},  // P0721
    {0x0722, "Getriebe-Ausgangsdrehzahl: kein Signal"},  // P0722
    {0x0725, "Motordrehzahl-Eingang (Getriebe): Stromkreis"},  // P0725
    {0x0730, "Falsche \303\234bersetzung (Getriebe)"},  // P0730
    {0x0731, "Gang 1: falsche \303\234bersetzung"},  // P0731
    {0x0732, "Gang 2: falsche \303\234bersetzung"},  // P0732
    {0x0733, "Gang 3: falsche \303\234bersetzung"},  // P0733
    {0x0734, "Gang 4: falsche \303\234bersetzung"},  // P0734
    {0x0735, "Gang 5: falsche \303\234bersetzung"},  // P0735
    {0x0740, "Wandlerkupplung: Stromkreis"},  // P0740
    {0x0741, "Wandlerkupplung: Funktion/klemmt offen"},  // P0741
    {0x0743, "Wandlerkupplung: elektrischer Fehler"},  // P0743
    {0x0745, "Getriebe-Druckregelventil: Stromkreis"},  // P0745
    {0x0750, "Schaltventil A: Stromkreis"},  // P0750
    {0x0751, "Schaltventil A: Funktion/klemmt"},  // P0751
    {0x0755, "Schaltventil B: Stromkreis"},  // P0755
    {0x0756, "Schaltventil B: Funktion/klemmt"},  // P0756
    {0x0760, "Schaltventil C: Stromkreis"},  // P0760
    {0x0765, "Schaltventil D: Stromkreis"},  // P0765
    {0x0770, "Schaltventil E: Stromkreis"},  // P0770
    {0x0780, "Schaltvorgang: Fehlfunktion"},  // P0780
    {0x0801, "R\303\274ckw\303\244rtsgangsperre: Stromkreis"},  // P0801
    {0x0830, "Kupplungsschalter A: Stromkreis"},  // P0830
    {0x0833, "Kupplungsschalter B: Stromkreis"},  // P0833
    {0x0850, "Park-/Neutralschalter: Stromkreis"},  // P0850
};

const int TABLE_SIZE = sizeof(TABLE) / sizeof(TABLE[0]);

}  // namespace dtc
