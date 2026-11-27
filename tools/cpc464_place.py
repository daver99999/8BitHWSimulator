"""cpc464_place.py - where the parts of the Amstrad CPC 464 main board (Z70100) sit, read off the overview photo of a real board and its close-ups.

Positions are in pixels of the overview photo (u across the board, v along its long axis); place() turns them into millimetres in the board's own frame:
the long edge horizontal, the lettering AMSTRAD at the lower left, the edge connectors along the top, chips with their notch to the left (pin 1 at the lower left
of each chip). The photo is a little tilted and the scale grows towards the camera (3.6 px/mm at the top, 4.1 at the RAM), both corrected here.
Chips and connectors are read from the photo to about 1-2 mm; the resistors and capacitors whose label could be read are where the photo shows them, the rest sit
beside their chip (see the NOTE entries)."""
import math

TILT = math.atan(0.084)
def _s(v): return 3.62 if v <= 900 else 3.62 + (v - 900) * 0.000932

def to_mm(u, v):
    c, s = math.cos(TILT), math.sin(TILT)
    u2, v2 = u * c - v * s, u * s + v * c
    # mm along the board: integrate dv / s(v)
    x, vv, step = 0.0, 300.0, 5.0
    while vv + step <= v2:
        x += step / _s(vv + step / 2); vv += step
    x += (v2 - vv) / _s(v2)
    return x, (-u2) / _s(v2), 0.0

# ICs: (u, v) of the middle of the chip, pins running along v, notch / pin 1 at the small-v end
IC = {
 'IC101': (711, 384), 'IC102': (945, 547), 'IC103': (833, 545), 'IC104': (722, 527), 'IC105': (731, 664), 'IC106': (1029, 736),
 'IC107': (959, 772), 'IC108': (843, 778), 'IC109': (744, 794), 'IC110': (758, 955), 'IC111': (977, 1026), 'IC112': (873, 1067),
 'IC113': (767, 1075), 'IC114': (882, 1202), 'IC115': (777, 1210), 'IC116': (1013, 1395), 'IC117': (927, 1338), 'IC118': (872, 1346),
 'IC119': (818, 1354), 'IC120': (764, 1362), 'IC121': (935, 1437), 'IC122': (879, 1444), 'IC123': (827, 1455), 'IC124': (768, 1463),
 'IC125': (931, 1548)}

# two-lead parts: (u, v, axis) the axis ('u' or 'v') is the direction the leads run along in the photo
TWO = {
 # keyboard pull-up banks beside the keyboard connector: two columns of five, leads along u
 'R115': (936, 365, 'u'), 'R123': (936, 379, 'u'), 'R122': (936, 392, 'u'), 'R121': (936, 405, 'u'), 'R114': (936, 418, 'u'),
 'R116': (1002, 365, 'u'), 'R120': (1002, 379, 'u'), 'R119': (1002, 392, 'u'), 'R117': (1002, 405, 'u'), 'R118': (1002, 418, 'u'),
 # video bank by the video jack (the second row from the edge, then the first)
 'R133': (1012, 1523, 'u'), 'R131': (1012, 1534, 'u'), 'R125': (1012, 1546, 'u'), 'R132': (1012, 1557, 'u'), 'R129': (1012, 1567, 'u'), 'R127': (1012, 1578, 'u'),
 'R126': (1075, 1509, 'u'), 'R128': (1075, 1519, 'u'), 'R130': (1075, 1531, 'u'), 'R134': (1075, 1542, 'u'), 'R136': (1075, 1553, 'u'),
 'R135': (1075, 1565, 'u'), 'R137': (1075, 1574, 'u'),
 # beside the edge fingers of the printer connector
 'R108': (1066, 806, 'u'), 'R105': (1066, 816, 'u'), 'R103': (1066, 833, 'u'), 'R104': (1066, 852, 'u'), 'R107': (1066, 868, 'u'), 'R106': (1066, 880, 'u'),
 # the five by the mounting boss near the expansion edge
 'R124': (960, 1171, 'u'), 'R109': (960, 1188, 'u'), 'R110': (960, 1205, 'u'), 'R102': (960, 1220, 'u'), 'R112': (960, 1237, 'u'),
 # the ones around the 153 / 132 / 32 chips
 'R146': (697, 565, 'v'), 'R147': (760, 590, 'v'), 'R148': (716, 723, 'v'), 'R149': (768, 723, 'v'), 'R150': (714, 840, 'v'),
 'R101': (745, 878, 'u'), 'D101': (745, 889, 'u'),
 'R151': (810, 1125, 'v'), 'R153': (793, 1125, 'v'), 'R152': (745, 1122, 'v'), 'R142': (789, 1294, 'u'),
 # the oscillator and tape corner
 'R143': (887, 1516, 'v'), 'R144': (890, 1568, 'v'), 'R145': (901, 1602, 'u'), 'R113': (830, 1518, 'v'),
 'R138': (1062, 1330, 'u'), 'R139': (1062, 1341, 'u'), 'R140': (1062, 1352, 'u'), 'R141': (1062, 1363, 'u'),
 'C129': (898, 1545, 'v'), 'C101': (964, 1580, 'v'), 'C102': (760, 863, 'v'),
 'C103': (985, 1482, 'u'), 'C130': (1010, 1470, 'u'), 'X101': (1000, 1490, 'v'),
 'LK1': (1030, 890, 'u'), 'LK2': (1030, 899, 'u'), 'LK3': (1030, 908, 'u'), 'LK4': (1030, 917, 'u'),
 # not on the drawing or not found on the board: stand-ins kept clear of the real parts
 'R600': (690, 1330, 'u'), 'R601': (690, 1345, 'u'), 'RX1': (1010, 1055, 'u'), 'PAD': (1020, 1250, 'u'),
}
# transistors (TO-92, three leads along v): TR1 is Q101, TR2 Q102, TR3 Q103
THREE = {'Q101': (795, 1567), 'Q102': (1033, 1507), 'Q103': (1070, 1375)}

# decoupling capacitors: next to their chip, at the board side where the photo shows the disc (u, v); C-numbers read from the board where legible
CAPS = {
 'C126': (1037, 805), 'C119': (970, 890), 'C127': (731, 549), 'C123': (741, 690), 'C122': (754, 840), 'C116': (879, 1119), 'C117': (768, 1102),
 'C104': (838, 640), 'C105': (764, 1419), 'C106': (822, 1411), 'C107': (881, 1396), 'C108': (937, 1390),
 'C109': (772, 1525), 'C110': (831, 1517), 'C111': (891, 1502), 'C112': (943, 1494),
 'C113': (718, 447), 'C114': (954, 628), 'C115': (864, 871), 'C118': (766, 1009), 'C120': (989, 1137), 'C121': (893, 1269),
 'C124': (789, 1267), 'C125': (1002, 1277), 'C128': (972, 1542)}

# connectors: (u, v) of the middle and how their pins lie
CONNS = {'CP002': (833, 303), 'J103': (1072, 422), 'J102': (1064, 507), 'J104': (1134, 1477), 'J101': (1150, 1568), 'CP001': (829, 1648)}
EDGE = {'PRN': (1100, 795), 'EXPC': (1112, 1140)}

def place(chips, names_of, base):
    """chips: ref -> (type, value, n); names_of(ref) -> the pad names; returns ref -> pads [(name, x, y, w, h)], and the board size"""
    out = {}
    def put(ref, pts):
        out[ref] = pts
    for ref, (u, v) in IC.items():
        typ, val, n = chips[ref]
        names = names_of(ref)
        pads, _ = base.pads_for('DIP-%d' % n, names, 0, 0)
        half = n // 2
        cx, cy, _ = to_mm(u, v)
        wr = base.dip_geom(n)
        clx, cly = wr / 2, (half - 1) * 1.27
        pts = [(nm, cx + (py - cly), cy - (px - clx), w, h) for (nm, px, py, w, h) in pads]
        put(ref, pts)
    def two(ref, u, v, ax, span):
        names = names_of(ref)
        cx, cy, _ = to_mm(u, v)
        dx, dy = (span / 2, 0) if ax == 'v' else (0, span / 2)
        if len(names) == 1: put(ref, [(names[0], cx, cy, 1.6, 1.6)]); return
        put(ref, [(names[0], cx - dx, cy + dy, 1.6, 1.6), (names[1], cx + dx, cy - dy, 1.6, 1.6)])
    for ref, (u, v, ax) in TWO.items():
        if ref not in chips: continue
        typ = chips[ref][0]
        two(ref, u, v, ax, 10.16 if typ == 'R' else 5.08 if typ in ('C', 'CE', 'XTAL', 'LINK', 'D') else 2.54 if typ == 'PAD' else 5.08)
    for ref, (u, v) in CAPS.items():
        if ref in chips: two(ref, u, v, 'v', 5.08)
    for ref, (u, v) in THREE.items():
        names = names_of(ref); cx, cy, _ = to_mm(u, v)
        put(ref, [(nm, cx + (i - 1) * 2.54, cy, 1.6, 1.6) for i, nm in enumerate(names)])
    # single-row connectors along the long axis' cross direction (pins run along the board's short side)
    for ref, (u, v) in CONNS.items():
        names = names_of(ref); n = len(names); cx, cy, _ = to_mm(u, v)
        pitch = 3.96 if ref == 'CP001' else 2.54
        if ref == 'CP002': pts = [(nm, cx, cy + (i - (n - 1) / 2) * pitch, 1.6, 1.6) for i, nm in enumerate(names)]
        elif ref == 'CP001': pts = [(nm, cx, cy + (i - (n - 1) / 2) * pitch, 1.6, 1.6) for i, nm in enumerate(names)]
        elif ref == 'J102': pts = [(nm, cx + (i % 2) * 2.77, cy + ((i // 2) - 2) * 2.77 * 2 / 2 * 1.0 + (0 if i % 2 == 0 else 1.385), 1.6, 1.6) for i, nm in enumerate(names)]
        else: pts = [(nm, cx + (i - (n - 1) / 2) * 5.0, cy + (0 if i % 2 == 0 else 3.0), 2.4, 2.4) for i, nm in enumerate(names)]
        put(ref, pts)
    for ref, (u, v) in EDGE.items():
        names = names_of(ref); n = len(names); cx, cy, _ = to_mm(u, v)
        half = (n + 1) // 2
        put(ref, [(nm, cx + ((i // 2) - (half - 1) / 2) * 2.54, cy + (i % 2) * 3.0, 1.8, 2.4) for i, nm in enumerate(names)])
    xs = [p[1] for pts in out.values() for p in pts]; ys = [p[2] for pts in out.values() for p in pts]
    ox, oy = min(xs) - 8, min(ys) - 8
    for ref in out: out[ref] = [(nm, x - ox, y - oy, w, h) for (nm, x, y, w, h) in out[ref]]
    return out, max(xs) - ox + 8, max(ys) - oy + 8
