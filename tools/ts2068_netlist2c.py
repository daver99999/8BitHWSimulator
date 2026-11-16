#!/usr/bin/env python3
"""ts2068_netlist2c.py - turn the TS2068 netlist dump (tools/ts2068-netlist-src/ts2068_netlist.json)
into the board table src/ts2068_board_data.c (bd_ts2068). Same format as the PET's.
The netlist was transcribed by hand from the schematic 335-870500 (no CAD source); parts the layout
drawing (placement.py) does not place go in a row below the board.
Usage: tools/ts2068_netlist2c.py [outdir]  (default: src)
"""
import os, re, sys
here = os.path.dirname(os.path.abspath(__file__))
src = os.path.join(here, 'ts2068-netlist-src')
sys.path.insert(0, src)
import pinlib
import placement

def read(path):
    chips, nets = {}, {}
    for line in open(path):
        line = line.strip()
        if line.startswith('chip '):
            _, ref, typ = line.split()
            chips[ref] = typ
        elif line.startswith('net '):
            name, rest = line[4:].split(':', 1)
            nets[name.strip()] = [m for m in rest.split()]
    return chips, nets

# (width mm between rows) for DIP by pin count
def dip_geom(n):
    return 15.24 if n >= 24 else 7.62

def footprint(ref, typ, npins):
    if typ in ('RES',): return 'R_Axial'
    if typ == 'CAP': return 'C_Disc'
    if typ in ('Q3', 'XTAL'): return 'TO-92' if typ == 'Q3' else 'XTAL'
    if typ == 'CONN': return 'CONN'
    if typ == 'RNET14': return 'DIP-14'   # a resistor pack in a DIP-14 case (bussed, the common is pin 14)
    return 'DIP-%d' % npins

def pad_layout(ref, typ, npins, x0, y0):
    """absolute pads (name, x, y, w, h) for a part whose top-left corner is at x0,y0; returns pads, (w,h)"""
    pads = []
    if footprint(ref, typ, npins).startswith('DIP'):   # RNET14 too
        half = npins // 2
        wr = dip_geom(npins)
        for i in range(1, npins + 1):
            if i <= half: x, y = x0, y0 + (i - 1) * 2.54
            else:         x, y = x0 + wr, y0 + (npins - i) * 2.54
            pads.append((str(i), x, y, 1.6, 1.6))
        return pads, (wr + 2, half * 2.54 + 2)
    if typ == 'CONN':
        # two rows of pins, pitch 2.54
        for i in range(1, npins + 1):
            pads.append((str(i), x0 + ((i - 1) % 2) * 2.54, y0 + ((i - 1) // 2) * 2.54, 1.6, 1.6))
        return pads, (6, ((npins + 1) // 2) * 2.54 + 1)
    for i in range(1, npins + 1):
        pads.append((str(i), x0 + (i - 1) * 2.54, y0, 1.6, 1.6))
    return pads, (npins * 2.54 + 1, 4)

def mm(px, py):
    return ((px - placement.ORIGIN[0]) / placement.SCALE, (py - placement.ORIGIN[1]) / placement.SCALE)

def pads_at(ref, typ, n):
    """pads of a part at its place on the real board (placement.py), or None if it has none"""
    P = placement
    if footprint(ref, typ, n).startswith('DIP') and ref in P.IC:
        cx, cy = mm(*P.IC[ref])
        half = n // 2
        wr = dip_geom(n)
        return pad_layout(ref, typ, n, cx - wr / 2, cy - (half - 1) * 2.54 / 2)[0]
    if ref in getattr(P, 'SQUARE', {}):
        # a 68 pin square (PLCC) package: pin 1 at the middle of the top side, counterclockwise
        cx, cy = mm(*P.SQUARE[ref])
        per = n // 4
        pitch, half = 1.27, (n // 4 - 1) / 2 * 1.27
        d = half + 3.0
        pads = []
        for i in range(1, n + 1):
            k = (i - 10) % n                       # pin 10 is the first of the left side (pin 1: middle of the top)
            side, j = divmod(k, per)
            a = (j - (per - 1) / 2) * pitch
            if side == 0:   x, y = cx - d, cy + a            # left side, downwards
            elif side == 1: x, y = cx + a, cy + d            # bottom, left to right
            elif side == 2: x, y = cx + d, cy - a            # right side, upwards
            else:           x, y = cx - a, cy - d            # top, right to left
            pads.append((str(i), x, y, 0.8, 0.8))
        return pads
    if ref in P.CONN:
        px, py, orient, rows = P.CONN[ref]
        cx, cy = mm(px, py)
        per = (n + rows - 1) // rows            # pads along the connector
        pads = []
        for i in range(1, n + 1):
            along, across = (i - 1) // rows, (i - 1) % rows
            a = (along - (per - 1) / 2) * 2.54
            b = (across - (rows - 1) / 2) * 2.54
            x, y = (cx + a, cy + b) if orient == 'h' else (cx + b, cy + a)
            pads.append((str(i), x, y, 1.6, 1.6))
        return pads
    if ref in P.TWO_PAD:
        e = P.TWO_PAD[ref]
        cx, cy = mm(e[0], e[1])
        d = 3.81
        pts = [(cx - d, cy), (cx + d, cy)] if len(e) > 2 else [(cx, cy - d), (cx, cy + d)]
        return [(str(i + 1), x, y, 1.6, 1.6) for i, (x, y) in enumerate(pts)][:n]
    if ref in P.OTHER:
        cx, cy = mm(*P.OTHER[ref])
        pitch = 5.08 if n > 2 else 7.62
        return [(str(i + 1), cx + (i - (n - 1) / 2) * pitch, cy, 2.0, 2.0) for i in range(n)]
    return None

def build(chips, nets):
    maxpin = {}
    padnet = {}
    # The simulator numbers pins up to 64 (PIN_NUMBERS): a connector's second row, drawn as 101-112 on the
    # schematic (J2, J8), continues after its first row (J2: 101 -> 13, J8: 101 -> 11).
    low = {}
    for name, members in nets.items():
        for m in members:
            ref, pin = m.rsplit('.', 1)
            if chips.get(ref) == 'CONN' and int(pin) < 100: low[ref] = max(low.get(ref, 0), int(pin))
    for ni, (name, members) in enumerate(sorted(nets.items())):
        for m in members:
            ref, pin = m.rsplit('.', 1)
            if chips.get(ref) == 'CONN' and int(pin) > 100: pin = str(int(pin) - 100 + low[ref])
            padnet[(ref, int(pin))] = name
            maxpin[ref] = max(maxpin.get(ref, 0), int(pin))
    comps = []
    for ref, typ in chips.items():
        if typ in pinlib.PINS and typ != 'CONN': n = len(pinlib.PINS[typ])
        elif typ == 'RES' or typ == 'CAP' or typ == 'XTAL': n = 2
        elif typ == 'BRIDGE': n = 4
        elif typ == 'Q3': n = 3
        else: n = maxpin.get(ref, 2)
        comps.append((ref, typ, n))
    # layout: ICs by reference in rows, small parts below
    def key(c):
        ref = c[0]
        m = re.match(r'([A-Z]+)(\d+)', ref)
        return (m.group(1), int(m.group(2))) if m else (ref, 0)
    ics = sorted([c for c in comps if c[1] not in ('RES', 'CAP', 'Q3', 'XTAL', 'CONN', 'RNET14', 'BRIDGE')], key=key)
    others = sorted([c for c in comps if c not in ics], key=key)
    placed = {}
    W, H = placement.SIZE[0] / placement.SCALE, placement.SIZE[1] / placement.SCALE
    x, y, rowh = 10.0, H + 12, 0.0      # parts the drawing doesn't place: a row below the board
    loose = []
    for c in ics + others:
        ref, typ, n = c
        pads = pads_at(ref, typ, n)
        if pads is not None:
            placed[ref] = pads
            continue
        loose.append(ref)
        pads, (w, h) = pad_layout(ref, typ, n, x, y)
        if x + w > W:
            x, y, rowh = 10.0, y + rowh + 8, 0.0
            pads, (w, h) = pad_layout(ref, typ, n, x, y)
        placed[ref] = pads
        x += w + 6
        rowh = max(rowh, h)
    if loose: print('not placed (put below the board):', ' '.join(loose))
    height = H
    netnames = sorted(nets)
    netidx = {n: i for i, n in enumerate(netnames)}
    return comps, placed, padnet, netnames, netidx, height

def emit(path, chips, nets, ident, title):
    comps, placed, padnet, netnames, netidx, height = build(chips, nets)
    out = []
    out.append('/* %s - GENERATED by tools/ts2068_netlist2c.py, do not edit.\n * %s: netlist transcribed by hand from the schematic 335-870500;\n * the ICs, J4 and J9 are placed from the component layout (manual p.353); the small parts sit in rows below the board. */\n' % (os.path.basename(path), title))
    out.append('#include "board_data.h"\n\nstatic const char *const nets[] = {')
    for n in netnames: out.append('    "%s",' % n.replace('\\', '\\\\').replace('"', '\\"'))
    out.append('};\n\nstatic const bd_pad_t pads[] = {')
    first, ncomp = {}, []
    k = 0
    for ref, typ, n in comps_sorted(comps):
        first[ref] = k
        for (name, x, y, w, h) in placed[ref]:
            net = padnet.get((ref, int(name)))
            out.append('    { "%s", %.2ff, %.2ff, %.2ff, %.2ff, %d },' % (name, x, y, w, h, netidx[net] if net else -1))
            k += 1
        ncomp.append((ref, typ, n))
    out.append('};\n\nstatic const bd_comp_t comps[] = {')
    for ref, typ, n in ncomp:
        out.append('    { "%s", "%s", "%s", %d, %d },' % (ref, typ, footprint(ref, typ, n), first[ref], n))
    out.append('};\n')
    out.append('const board_data_t bd_%s = {\n    { 0, 0, %.0f, %.0f },\n    nets, %d, pads, %d, comps, %d };\n' %
               (ident, placement.SIZE[0] / placement.SCALE, height, len(netnames), k, len(ncomp)))
    open(path, 'w').write('\n'.join(out))
    print(path, len(comps), 'parts', len(netnames), 'nets', k, 'pads')

def comps_sorted(comps):
    def key(c):
        m = re.match(r'([A-Z]+)(\d+)', c[0])
        return (m.group(1), int(m.group(2))) if m else (c[0], 0)
    return sorted(comps, key=key)

if __name__ == '__main__':
    outdir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, '..', 'src')
    chips, nets = read(os.path.join(src, 'ts2068_netlist.json'))
    emit(os.path.join(outdir, 'ts2068_board_data.c'), chips, nets, 'ts2068',
         'Timex Sinclair TS2068 (board 335-850001, schematic 335-870500)')
