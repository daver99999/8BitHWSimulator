#!/usr/bin/env python3
"""apple2_sch2net.py - trace the KiCad schematic sheets of the Apple II+ (baldengineer/bit-preserve, Apple/Apple II+/AppleII+)
into a netlist: tools/apple2plus-netlist-src/apple2plus.net ('chip REF TYPE' and 'net NAME: REF.PIN ...' lines).
Usage: tools/apple2_sch2net.py <dir with the .kicad_sch files> [out.net]
Connectivity as KiCad has it: wire ends, pins, labels, junctions, sheet pins that touch a wire (end or middle) are one net;
local and hierarchical labels are per sheet, power symbols are global, a hierarchical label joins the pin of the same name on the
sheet symbol one level up; buses are only drawings (every wire to a bus entry carries a label). Hidden power pins
(VCC, GND) join +5V and GND.
"""
import math, os, re, sys
from collections import defaultdict

def tokenize(text):
    return re.findall(r'\(|\)|"(?:[^"\\]|\\.)*"|[^\s()"]+', text)

def parse(text):
    stack = [[]]
    for t in tokenize(text):
        if t == '(':
            stack.append([])
        elif t == ')':
            l = stack.pop(); stack[-1].append(l)
        elif t[0] == '"':
            stack[-1].append(('s', t[1:-1].replace('\\"', '"')))
        else:
            stack[-1].append(t)
    return stack[0][0]

def head(n): return n[0] if n and isinstance(n[0], str) else None
def sub(n, name): return [c for c in n[1:] if isinstance(c, list) and head(c) == name]
def first(n, name):
    r = sub(n, name); return r[0] if r else None
def sval(x): return x[1] if isinstance(x, tuple) else x

def prop(n, key):
    for p in sub(n, 'property'):
        if sval(p[1]) == key: return sval(p[2])
    return None

class Sheet:
    pass

def load_sheet(path):
    root = parse(open(path).read())
    sh = Sheet()
    sh.name = os.path.basename(path)
    libs = {}
    for ls in sub(first(root, 'lib_symbols'), 'symbol'):
        name = sval(ls[1])
        pins = []   # (unit, style, number, pname, type, hidden, x, y)
        def walk(node, unit, style):
            for c in node[2:]:
                if isinstance(c, list) and head(c) == 'symbol':
                    m = re.search(r'_(\d+)_(\d+)$', sval(c[1]))
                    walk(c, int(m.group(1)), int(m.group(2)))
                elif isinstance(c, list) and head(c) == 'pin':
                    at = first(c, 'at')
                    hidden = 'hide' in c or any(isinstance(d, list) and head(d) == 'hide' for d in c)
                    nm = first(c, 'name'); nu = first(c, 'number')
                    pins.append((unit, style, sval(nu[1]), sval(nm[1]), c[1], hidden, float(at[1]), float(at[2])))
        walk(ls, 0, 0)
        libs[name] = pins
    sh.libs = libs
    sh.syms = []
    for s in sub(root, 'symbol'):
        lib = sval(first(s, 'lib_id')[1])
        at = first(s, 'at')
        mir = first(s, 'mirror')
        unit = first(s, 'unit')
        ref = prop(s, 'Reference')
        inst = first(s, 'instances')
        if inst:
            for pr in sub(inst, 'project'):
                for pa in sub(pr, 'path'):
                    r = first(pa, 'reference')
                    if r: ref = sval(r[1])
        sh.syms.append(dict(lib=lib, x=float(at[1]), y=float(at[2]), rot=int(float(at[3])) if len(at) > 3 else 0,
                            mirror=mir[1] if mir else None, unit=int(unit[1]) if unit else 1,
                            ref=ref, prop_ref=prop(s, 'Reference'), value=prop(s, 'Value'), fp=prop(s, 'Footprint')))
    sh.wires = []
    for w in sub(root, 'wire'):
        pts = first(w, 'pts'); xy = sub(pts, 'xy')
        sh.wires.append(((float(xy[0][1]), float(xy[0][2])), (float(xy[1][1]), float(xy[1][2]))))
    sh.junctions = [(float(first(j, 'at')[1]), float(first(j, 'at')[2])) for j in sub(root, 'junction')]
    sh.labels = [(sval(l[1]), float(first(l, 'at')[1]), float(first(l, 'at')[2]), head(l)) for k in ('label', 'hierarchical_label', 'global_label') for l in sub(root, k)]
    sh.noconn = [(float(first(j, 'at')[1]), float(first(j, 'at')[2])) for j in sub(root, 'no_connect')]
    sh.sheets = []
    for s in sub(root, 'sheet'):
        pins = [(sval(p[1]), float(first(p, 'at')[1]), float(first(p, 'at')[2])) for p in sub(s, 'pin')]
        sh.sheets.append(dict(name=prop(s, 'Sheetname') or prop(s, 'Sheet name'), file=prop(s, 'Sheetfile') or prop(s, 'Sheet file'), pins=pins))
    return sh

def pin_pos(sym, p):
    """absolute position of a lib pin (lib coordinates are Y up) for an instance with rotation and mirror"""
    x, y = p[6], p[7]
    if sym['mirror'] == 'x': y = -y
    elif sym['mirror'] == 'y': x = -x
    a = math.radians(sym['rot'])
    rx = x * math.cos(a) - y * math.sin(a)
    ry = x * math.sin(a) + y * math.cos(a)
    return (round(sym['x'] + rx, 3), round(sym['y'] - ry, 3))

def key(pt): return (round(pt[0], 2), round(pt[1], 2))

class UF:
    def __init__(self): self.p = {}
    def find(self, a):
        self.p.setdefault(a, a)
        while self.p[a] != a:
            self.p[a] = self.p[self.p[a]]; a = self.p[a]
        return a
    def union(self, a, b):
        a, b = self.find(a), self.find(b)
        if a != b: self.p[a] = b

def on_seg(pt, a, b):
    (x, y), (x1, y1), (x2, y2) = pt, a, b
    if abs((x2 - x1) * (y - y1) - (y2 - y1) * (x - x1)) > 0.02 * (abs(x2 - x1) + abs(y2 - y1) + 1): return False
    return min(x1, x2) - 0.01 <= x <= max(x1, x2) + 0.01 and min(y1, y2) - 0.01 <= y <= max(y1, y2) + 0.01

# DIP size of the ICs whose symbols leave the supply pins out (VCC on the last pin, GND on the middle one)
PKG = {'74LS00': 14, '74LS02': 14, '74LS04': 14, '74LS08': 14, '74LS11': 14, '74LS20': 14, '74LS32': 14, '74LS51': 14, '74LS74': 14,
       '74LS86': 14, '74LS138': 16, '74LS139': 16, '74LS151': 16, '74LS153': 16, '74LS161': 16, '74LS166': 16, '74LS174': 16,
       '74LS175': 16, '74LS194': 16, '74LS195': 16, '74LS251': 16, '74LS257': 16, '74LS259': 16, '74LS283': 16, '8T97': 16, 'DP8304': 20}
POWER = {'GND': 'GND', '+5V': '+5V', '-5V': '-5V', '+12V': '+12V', '-12V': '-12V', 'VCC': '+5V'}

FIXUPS = [('RA2.2', '~{INH}'), ('B5.1', 'SOFT5'), ('B8.1', 'SOFT5')]


def main():
    d = sys.argv[1]
    topfile = 'AppleII+.kicad_sch'
    if d.endswith('.kicad_sch'):        # any KiCad 6+ project: the top sheet's file
        d, topfile = os.path.split(d)
    fixups = FIXUPS if topfile == 'AppleII+.kicad_sch' else []
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'apple2plus-netlist-src', 'apple2plus.net')
    top = load_sheet(os.path.join(d, topfile))
    sheets = {}
    for s in top.sheets:
        sheets[s['name']] = (s, load_sheet(os.path.join(d, s['file'])))
    uf = UF()
    chips = {}          # ref -> (type, value, footprint)
    pins_of = defaultdict(list)
    warn = []
    def process(sh, scope, parent_pins):
        """add the nodes of a sheet; node keys are (scope, x, y)"""
        wires = sh.wires
        pts = defaultdict(list)
        def node(pt): return (scope,) + key(pt)
        for a, b in wires:
            uf.union(node(a), node(b))
        allpoints = []
        for sym in sh.syms:
            lib = sh.libs.get(sym['lib'])
            if lib is None: warn.append('no lib ' + sym['lib']); continue
            is_power = sym['lib'].startswith('power:') or (sym['ref'] or '').startswith('#')
            for p in lib:
                if p[0] not in (0, sym['unit']): continue
                if p[1] not in (0, 1): continue
                pos = pin_pos(sym, p)
                if is_power:
                    nm = sym['value']
                    if nm == 'PWR_FLAG': continue
                    uf.union(node(pos), ('net', POWER.get(nm, nm)))
                    allpoints.append(node(pos));
                else:
                    pins_of[sym['ref']].append((p[2], p[3], p[4], p[5], node(pos), scope, sym['unit']))
                    allpoints.append(node(pos))
            if not is_power:
                chips[sym['ref']] = (sym['lib'], sym['value'], sym['fp'])
        for (x, y) in sh.junctions: allpoints.append(node((x, y)))
        labelpts = []
        for (name, x, y, kind) in sh.labels:
            n = node((x, y)); allpoints.append(n)
            if kind == 'hierarchical_label':
                uf.union(n, ('hier', scope, name))
            else:
                uf.union(n, ('lab', scope, name))
        for (x, y) in sh.noconn: allpoints.append(node((x, y)))
        # a point on the middle of a wire joins it
        for pt in allpoints:
            p = (pt[1], pt[2])
            for a, b in wires:
                if on_seg(p, key(a), key(b)): uf.union(pt, node(a)); break
        # wire ends on the middle of another wire
        for a, b in wires:
            for e in (a, b):
                for c, dd in wires:
                    if (c, dd) != (a, b) and on_seg(key(e), key(c), key(dd)): uf.union(node(e), node(c))
        # sheet symbols: their pins join the hierarchical labels of the sheet below
        for s in sh.sheets:
            sp = sheets[s['name']][1]
            for (pname, x, y) in s['pins']:
                n = node((x, y))
                for w in wires:
                    if on_seg((n[1], n[2]), key(w[0]), key(w[1])): uf.union(n, node(w[0])); break
                uf.union(n, ('hier', scope + '/' + s['name'], pname))
    process(top, 'top', None)
    for name, (s, sh) in sheets.items():
        process(sh, 'top/' + name, None)
    # names: power nets win, then labels (hierarchical names are reached by label name in the top scope)
    groups = defaultdict(list)
    for k in list(uf.p): groups[uf.find(k)].append(k)
    rootname = {}
    for r, ks in groups.items():
        names = [k[1] for k in ks if k[0] == 'net']
        labs = sorted(set(k[2] for k in ks if k[0] == 'lab')) + sorted(set(k[2] for k in ks if k[0] == 'hier'))
        rootname[r] = names[0] if names else (labs[0] if labs else None)
    nets = defaultdict(list)
    unnamed = {}
    for ref, plist in pins_of.items():
        if ref is None: continue
        for (num, pname, ptype, hidden, nd, scope, unit) in plist:
            if hidden and ptype in ('power_in', 'power_out'):
                nm = POWER.get(pname, POWER.get(pname.upper()))
                if nm is None: warn.append('hidden pin %s %s %s' % (ref, num, pname)); continue
                nets[nm].append('%s.%s' % (ref, num)); continue
            r = uf.find(nd)
            nm = rootname.get(r)
            if nm is None:
                nm = unnamed.setdefault(r, 'N$%d' % (len(unnamed) + 1))
            nets[nm].append('%s.%s' % (ref, num))
    # the supply pins the symbols leave out
    have = {m for ms in nets.values() for m in ms}
    for ref, (lib, val, fp) in chips.items():
        n = PKG.get(lib.split(':')[-1])
        if n and ref in pins_of:
            for pin, nm in ((n, '+5V'), (n // 2, 'GND')):
                if '%s.%d' % (ref, pin) not in have: nets[nm].append('%s.%d' % (ref, pin))
    # pins the drawing leaves unwired (the pull-up of /INH is a pin of RA2 that goes nowhere on the sheet)
    for pin_, nm in fixups:
        for ms in nets.values():
            while pin_ in ms: ms.remove(pin_)
        nets[nm].append(pin_)
    # a pin on two nets is a drawing mistake
    seen = {}
    for nm, ms in nets.items():
        for m in set(ms):
            if m in seen and seen[m] != nm: warn.append('pin %s on %s and %s' % (m, seen[m], nm))
            seen[m] = nm
    # nets with a single pin are not nets
    lines = []
    for ref in sorted(chips, key=lambda r: (re.match(r'[A-Za-z#]+', r).group(0), int(re.search(r'\d+', r).group(0)) if re.search(r'\d+', r) else 0)):
        if ref.startswith('#') or ref not in pins_of: continue
        lib, val, fp = chips[ref]
        npin = PKG.get(lib.split(':')[-1]) or len({p[2] for sh in [top] + [x[1] for x in sheets.values()] if lib in sh.libs for p in sh.libs[lib]})
        lines.append('chip %s %s|%s|%d' % (ref, lib.split(':')[-1], val, npin))
    for nm in sorted(nets):
        mem = sorted(set(nets[nm]), key=lambda m: (m.rsplit('.', 1)[0], m.rsplit('.', 1)[1]))
        lines.append('net %s: %s' % (nm, ' '.join(mem)))
    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, 'w').write('\n'.join(lines) + '\n')
    print(out, len([l for l in lines if l.startswith('chip')]), 'parts', len(nets), 'nets,', len(unnamed), 'unnamed')
    for w in sorted(set(warn)): print('warning:', w)

if __name__ == '__main__':
    main()
