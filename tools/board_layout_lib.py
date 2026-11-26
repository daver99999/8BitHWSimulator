"""board_layout_lib.py - helpers shared by the board generators of the Apple tab: read a netlist (tools/<board>-netlist-src/*.net,
made by apple2_sch2net.py), name the pads of a part and give them their places for a DIP, a card slot or a row of leads."""
import re

def read(path):
    chips, nets = {}, {}
    for line in open(path):
        line = line.rstrip('\n')
        if line.startswith('chip '):
            _, ref, rest = line.split(' ', 2)
            typ, val, n = rest.split('|')
            chips[ref] = (typ, val, int(n))
        elif line.startswith('net '):
            name, rest = line[4:].split(':', 1)
            nets[name.strip()] = rest.split()
    return chips, nets

def dip_geom(n): return 15.24 if n >= 24 else 7.62

def pad_names(typ, n, pins):
    """the pad names of a part: 1..n, or the names the schematic uses (S, T of a jack)"""
    nums = sorted(pins, key=lambda p: (not p.isdigit(), int(p) if p.isdigit() else 0, p))
    if all(p.isdigit() for p in nums) or not nums:
        return [str(i) for i in range(1, max(n, max((int(p) for p in nums), default=0)) + 1)]
    return nums

def pads_for(fp, names, x0, y0):
    n = len(names)
    pads = []
    if fp.startswith('DIP'):
        half = n // 2
        wr = dip_geom(n)
        for i, nm in enumerate(names, 1):
            if i <= half: x, y = x0, y0 + (i - 1) * 2.54
            else:         x, y = x0 + wr, y0 + (n - i) * 2.54
            pads.append((nm, x, y, 1.6, 1.6))
        return pads, (wr + 2, half * 2.54 + 2)
    if fp == 'CONN':
        # a card slot: two rows of pins
        for i, nm in enumerate(names):
            pads.append((nm, x0 + (i % 2) * 2.54, y0 + (i // 2) * 2.54, 1.6, 1.6))
        return pads, (6, ((n + 1) // 2) * 2.54 + 1)
    for i, nm in enumerate(names):
        pads.append((nm, x0 + i * 2.54, y0, 1.6, 1.6))
    return pads, (n * 2.54 + 1, 4)

def sortkey(ref):
    m = re.match(r'([A-Za-z\-]+?)(\d+)?$', ref)
    return (m.group(1), int(m.group(2) or 0)) if m else (ref, 0)

