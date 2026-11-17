#!/usr/bin/env python3
"""vic20a_from_b.py - the early VIC-20 board (schematic 324001, "UL configuration", photo MDK 25HB)
from the 250403 data.

usage: python3 tools/vic20a_from_b.py src/vic20b_board_data.c > src/vic20a_board_data.c

The circuit of the early board is the 250403's with the memory of schematic 324001 sheet c: eleven 2114s
(UE1 the color RAM; the pairs UD2/UE2 at $0000, UD3/UE3 ... UD6/UE6 at $1000, $1400, $1800 and $1C00; the
UD chips take BD0-BD3, the UE chips BD4-BD7) selected by the outputs 0 and 4-7 of the 74LS138 UC4, instead
of the two 6116s and the 74LS08 U13. Everything else is taken from the 250403 as it is.

The positions: the chips and the connectors are put where the photo of the 25HB board has them (about 6 pixels
to the millimeter), with the pins numbered as on a DIP. The passives are NOT placed from the photo: each one
is put in a free place next to the chip it is connected to (their references are the 250403's)."""
import math
import re
import sys

sys.path.insert(0, __file__.rsplit('/', 1)[0] if '/' in __file__ else '.')

RAILS = {'+5V', 'GND', '+5VP', '9VACin', '9VACout'}

# Chip centers in millimeters on the 25HB (photo: mm = pixel / 6 at the full 2576 x 1472 size), and the
# direction: 'v' pins in two columns (pin 1 top left), 'h' pins in two rows (pin 1 bottom left).
ICS = {
    'UAB1': (30.7, 51.7, 'v'), 'UAB3': (60.7, 51.7, 'v'),
    'UB4': (82.3, 62.7, 'v'), 'UB6': (111.3, 65.0, 'v'),
    'UC2': (45.7, 90.0, 'v'), 'UC3': (64.0, 90.0, 'v'), 'UC4': (81.0, 90.0, 'v'),
    'UC5': (100.0, 88.3, 'v'), 'UC6': (118.3, 88.3, 'v'),
    'UD1': (26.7, 123.3, 'v'),
    'UD2': (45.0, 121.7, 'v'), 'UD3': (63.3, 121.7, 'v'), 'UD4': (80.7, 121.7, 'v'),
    'UD5': (100.0, 121.7, 'v'), 'UD6': (118.3, 121.7, 'v'),
    'UE1': (26.7, 148.3, 'v'), 'UE2': (45.0, 148.3, 'v'), 'UE3': (63.3, 148.3, 'v'),
    'UE4': (80.7, 148.3, 'v'), 'UE5': (100.0, 148.3, 'v'), 'UE6': (118.3, 148.3, 'v'),
    'UD7': (147.7, 125.0, 'v'),
    'UD8': (181.7, 115.7, 'h'), 'UE8': (181.7, 132.7, 'h'), 'UF8': (181.7, 152.3, 'h'),
    'UD9': (207.7, 120.7, 'v'),
    'UE10': (241.7, 145.0, 'h'),
    'UE11': (286.7, 148.3, 'v'), 'UE12': (309.3, 148.3, 'v'),
    'UB7': (172.0, 55.0, 'h'),      # under the shield
    'UB9': (205.0, 75.0, 'v'),
}
# Other parts with a place in the photo (centers).
FIXED = {
    'P10': (8.3, 52.0), 'P1': (46.7, 7.3), 'P2': (123.3, 6.7), 'P3': (171.7, 8.3), 'P4': (200.0, 8.3),
    'P5': (395.0, 91.7), 'P7': (400.0, 36.7), 'P6': (296.0, 4.0), 'SW1': (401.7, 60.0), 'F1': (368.3, 58.3),
    'CN1': (29.3, 15.0), 'Q4': (293.0, 83.0), 'C39': (239.0, 77.0), 'CR2': (350.0, 100.0), 'Y1': (205.0, 88.0),
}
OUTLINE = (0.0, 0.0, 408.0, 169.0)
REMOVED = {'U13', 'U14', 'U15', 'C24', 'C46'}      # the 6116s, their AND gate and their decoupling
RAM_PAIRS = {'UE3': 'O4', 'UD3': 'O4', 'UE4': 'O5', 'UD4': 'O5', 'UE5': 'O6', 'UD5': 'O6', 'UE6': 'O7', 'UD6': 'O7'}


def load(fn):
    t = open(fn).read()
    nets = re.findall(r'^    "((?:[^"\\]|\\.)*)",$', t.split('static const bd_pad_t')[0], re.M)
    pads_txt = t.split('static const bd_pad_t')[1].split('static const bd_comp_t')[0]
    pads = re.findall(r'\{\s*"([^"]*)",\s*([-\d.e]+)f?,\s*([-\d.e]+)f?,\s*([-\d.e]+)f?,\s*([-\d.e]+)f?,\s*(-?\d+)\s*\}', pads_txt)
    comps = re.findall(r'\{\s*"([^"]*)",\s*"((?:[^"\\]|\\.)*)",\s*"([^"]*)",\s*(\d+),\s*(\d+)\s*\}',
                       t.split('static const bd_comp_t')[1])
    out = []
    for ref, value, fp, first, n in comps:
        ps = []
        for i in range(int(first), int(first) + int(n)):
            name, x, y, w, h, net = pads[i]
            ps.append([name, float(x), float(y), float(w), float(h), nets[int(net)] if int(net) >= 0 else None])
        out.append([ref, value, fp, ps])
    return out


def bbox(pads):
    x0 = min(p[1] - p[3] / 2 for p in pads); x1 = max(p[1] + p[3] / 2 for p in pads)
    y0 = min(p[2] - p[4] / 2 for p in pads); y1 = max(p[2] + p[4] / 2 for p in pads)
    return x0, y0, x1, y1


def center(pads):
    x0, y0, x1, y1 = bbox(pads)
    return (x0 + x1) / 2, (y0 + y1) / 2


def move_to(pads, cx, cy):
    ox, oy = center(pads)
    for p in pads:
        p[1] += cx - ox
        p[2] += cy - oy


def dip(comp, cx, cy, direction):
    """The pads of a DIP at its place."""
    m = re.match(r'DIP-(\d+)_W([\d.]+)mm', comp[2])
    n, width = int(m.group(1)), float(m.group(2))
    half = n // 2
    first = comp[3][0]
    for i, p in enumerate(comp[3]):
        pin = int(p[0])
        if pin <= half:
            x, y = -width / 2, (pin - 1 - (half - 1) / 2) * 2.54
        else:
            x, y = width / 2, ((half - 1) / 2 - (pin - half - 1)) * 2.54
        if direction == 'h':
            x, y = y, -x
        p[1], p[2] = cx + x, cy + y
        p[3], p[4] = first[3], first[4]


def overlaps(r, rects, margin):
    for q in rects:
        if r[0] < q[2] + margin and q[0] < r[2] + margin and r[1] < q[3] + margin and q[1] < r[3] + margin:
            return True
    return False


def main():
    comps = [c for c in load(sys.argv[1]) if c[0] not in REMOVED]
    by_ref = {c[0]: c for c in comps}

    # The memory of schematic 324001 sheet c: the pairs of 2114s on the outputs of UC4.
    for ref, out in RAM_PAIRS.items():
        src = by_ref['UD2' if ref.startswith('UD') else 'UE2']
        pads = [list(p) for p in src[3]]
        for p in pads:
            if p[0] == '8':
                p[5] = 'Net-(UC4-%s)' % out
        comps.append([ref, '2114', src[2], pads])
    by_ref = {c[0]: c for c in comps}

    # Original centers (for "the nearest chip").
    orig = {c[0]: center(c[3]) for c in comps}
    ics = [c for c in comps if c[0] in ICS]

    # Which chips a net touches.
    net_ics = {}
    for c in comps:
        if c[0] in ICS:
            for p in c[3]:
                if p[5] and p[5] not in RAILS:
                    net_ics.setdefault(p[5], []).append(c[0])

    rects = []
    placed = {}
    for ref, (cx, cy, d) in ICS.items():
        dip(by_ref[ref], cx, cy, d)
        placed[ref] = (cx, cy)
        x0, y0, x1, y1 = bbox(by_ref[ref][3])
        rects.append((x0 - 1.5, y0 - 1.5, x1 + 1.5, y1 + 1.5))
    for ref, (cx, cy) in FIXED.items():
        if ref not in by_ref:
            continue
        move_to(by_ref[ref][3], cx, cy)
        placed[ref] = (cx, cy)
        x0, y0, x1, y1 = bbox(by_ref[ref][3])
        rects.append((x0, y0, x1, y1))

    # The rest: in free space next to the chip they are connected to (most: the nearest in the 250403).
    for c in comps:
        ref = c[0]
        if ref in placed:
            continue
        count = {}
        for p in c[3]:
            for i in net_ics.get(p[5], []):
                count[i] = count.get(i, 0) + 1
        ox, oy = orig[ref]
        if count:
            best = max(count.values())
            anchor = min((i for i in count if count[i] == best),
                         key=lambda i: math.hypot(orig[i][0] - ox, orig[i][1] - oy))
        else:
            anchor = min((i['ref'] if isinstance(i, dict) else i[0] for i in ics),
                         key=lambda i: math.hypot(orig[i][0] - ox, orig[i][1] - oy))
        ax, ay = placed[anchor]
        x0, y0, x1, y1 = bbox(c[3])
        w, h = x1 - x0, y1 - y0
        cands = sorted(((ax + dx * 2.5, ay + dy * 2.5) for dx in range(-24, 25) for dy in range(-24, 25)),
                       key=lambda q: math.hypot(q[0] - ax, q[1] - ay))
        spot = None
        for qx, qy in cands:
            r = (qx - w / 2, qy - h / 2, qx + w / 2, qy + h / 2)
            if r[0] < OUTLINE[0] + 2 or r[1] < OUTLINE[1] + 2 or r[2] > OUTLINE[2] - 2 or r[3] > OUTLINE[3] - 2:
                continue
            if not overlaps(r, rects, 0.8):
                spot = (qx, qy)
                break
        if spot is None:
            sys.exit('no place for %s' % ref)
        move_to(c[3], *spot)
        placed[ref] = spot
        rects.append((spot[0] - w / 2, spot[1] - h / 2, spot[0] + w / 2, spot[1] + h / 2))

    # Nets in use, in the order of their first pad.
    nets = []
    for c in comps:
        for p in c[3]:
            if p[5] and p[5] not in nets:
                nets.append(p[5])
    index = {n: i for i, n in enumerate(nets)}

    out = sys.stdout
    out.write('/* vic20a_board_data.c - GENERATED by tools/vic20a_from_b.py, do not edit.\n')
    out.write(' * Early VIC-20 (schematic 324001, photo MDK 25HB): the 250403 circuit with the memory of sheet c (eleven\n')
    out.write(' * 2114s). Chips and connectors are placed as in the photo of the 25HB; the passives are only near their chips. */\n')
    out.write('#include "board_data.h"\n\nstatic const char *const nets[] = {\n')
    for n in nets:
        out.write('    "%s",\n' % n.replace('\\', '\\\\').replace('"', '\\"'))
    out.write('};\n\nstatic const bd_pad_t pads[] = {\n')
    for c in comps:
        for p in c[3]:
            out.write('    { "%s", %.3ff, %.3ff, %.3ff, %.3ff, %d },\n' % (
                p[0], p[1], p[2], p[3], p[4], index[p[5]] if p[5] else -1))
    out.write('};\n\nstatic const bd_comp_t comps[] = {\n')
    first = 0
    for c in comps:
        out.write('    { "%s", "%s", "%s", %d, %d },\n' % (
            c[0], c[1].replace('\\', '\\\\').replace('"', '\\"'), c[2], first, len(c[3])))
        first += len(c[3])
    out.write('};\n\nconst board_data_t bd_vic20a = {\n    { %.3ff, %.3ff, %.3ff, %.3ff },\n' % OUTLINE)
    out.write('    nets, %d, pads, %d, comps, %d,\n};\n' % (len(nets), first, len(comps)))


if __name__ == '__main__':
    main()
