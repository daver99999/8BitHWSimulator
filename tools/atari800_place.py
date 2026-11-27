"""atari800_place.py - the CPU card of the Atari 800 (the board marked 5, with the 6502, GTIA and ANTIC) placed from photos of a real card.
Positions are pixels of the top photo of the card (the card edge to the right, 13 px/mm), the part under the first photo's lower edge read from the second photo
(shifted 30 px right and 470 px down to the first photo's frame). All the chips sit rotated 180 degrees: pin 1 at the top right, pins 1..n/2 running left along the top row.
Chips and the edge connector are read to about 1-2 mm, the resistors and capacitors where their labels could be read; R308 and R311 were not found
and sit beside their neighbours."""
S = 13.0
IC = {'Z302': (705, 235), 'Z301': (994, 250), 'Z303': (770, 490), 'A303': (648, 700), 'Z304': (480, 962), 'A301': (615, 1470), 'A302': (655, 2030)}
# two-lead parts: (x, y, direction of the leads 'h' or 'v')
TWO = {'C305': (470, 160, 'h'), 'C301': (470, 205, 'h'), 'R309': (470, 340, 'v'), 'R312': (440, 505, 'h'), 'R313': (430, 862, 'h'), 'R310': (385, 1280, 'h'),
       'R307': (1010, 455, 'v'), 'C307': (1048, 480, 'v'), 'C306': (1085, 470, 'v'), 'C304': (275, 650, 'v'), 'C302': (255, 1460, 'v'), 'C303': (235, 1975, 'v'),
       'R305': (410, 1690, 'h'), 'R304': (410, 1735, 'h'), 'R303': (410, 1790, 'h'), 'R302': (415, 1835, 'h'), 'R306': (670, 1665, 'h'),
       'R308': (400, 2175, 'h'), 'R311': (780, 880, 'h')}
EDGE = ('J101C', 1165, 440, 1750)      # the edge connector: fingers between these y, at this x

def place(x0, y0, chips, names_of, base):
    """returns pads of the CPU card parts moved to (x0, y0) of the sheet, and its height"""
    out = {}
    xs0, ys0 = 215, 95          # the card's top left in the photo
    def mm(px, py): return x0 + (px - xs0) / S, y0 + (py - ys0) / S
    for ref, (px, py) in IC.items():
        typ, val, n = chips[ref]
        names = names_of(ref)
        pads, _ = base.pads_for('DIP-%d' % n, names, 0, 0)
        half = n // 2; wr = base.dip_geom(n)
        clx, cly = wr / 2, (half - 1) * 1.27
        cx, cy = mm(px, py)
        out[ref] = [(nm, cx - (ly - cly), cy + (lx - clx), w, h) for (nm, lx, ly, w, h) in pads]
    for ref, (px, py, d) in TWO.items():
        if ref not in chips: continue
        names = names_of(ref); cx, cy = mm(px, py)
        span = 10.16 if chips[ref][0] == 'R' else 5.08
        dx, dy = (span / 2, 0) if d == 'h' else (0, span / 2)
        out[ref] = [(names[0], cx - dx, cy - dy, 1.6, 1.6), (names[1], cx + dx, cy + dy, 1.6, 1.6)]
    ref, ex, ey0, ey1 = EDGE
    names = names_of(ref); n = len(names); half = (n + 1) // 2
    cx, _ = mm(ex, 0)
    pts = []
    for i, nm in enumerate(names):
        row = i // 2
        _, yy = mm(0, ey0 + (ey1 - ey0) * row / (half - 1))
        pts.append((nm, cx + (i % 2) * 3.0 - 1.5, yy, 2.4, 1.8))
    out[ref] = pts
    h = max(p[2] for pts in out.values() for p in pts) - y0 + 4
    return out, h
