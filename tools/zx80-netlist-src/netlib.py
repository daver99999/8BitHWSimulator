"""Netlist entry + rules check for the TS1000 reconstruction.

Usage (in a sheet file):
    from netlib import N
    N.chip('UD6', '2332')
    N.pins('UD6', {8: 'BA0', 7: 'BA1', ...})   # pin number -> net name
Nets are joined by name.  Rails: +5V GND +12V -5V.
Power pins of ICs are connected automatically (see pinlib.PWR); anything the
schematic shows differently should be entered explicitly.
"""
import collections
from pinlib import PINS, PWR

RAILS = {'+5V', 'GND', '+12V', '-5V', '+9V_UNREG', 'CHASSIS'}


class Netlist:
    def __init__(self):
        self.chips = {}                       # ref -> type
        self.conn = {}                        # (ref, pin) -> net
        self.sheet = {}                       # (ref, pin) -> sheet that entered it
        self.cur = '?'
        self.notes = []

    def sheet_is(self, n):
        self.cur = n

    def chip(self, ref, typ, note=None):
        assert typ in PINS, 'unknown type %s' % typ
        if ref in self.chips and self.chips[ref] != typ:
            raise ValueError('%s redefined %s -> %s' % (ref, self.chips[ref], typ))
        self.chips[ref] = typ

    def pins(self, ref, mapping):
        for pin, net in mapping.items():
            key = (ref, int(pin))
            if key in self.conn and self.conn[key] != net:
                raise ValueError('%s.%d already on %s, now %s (sheet %s, was %s)' %
                                 (ref, pin, self.conn[key], net, self.cur, self.sheet[key]))
            self.conn[key] = net
            self.sheet[key] = self.cur

    def note(self, text):
        self.notes.append((self.cur, text))

    # -------- queries --------
    def nets(self):
        d = collections.defaultdict(list)
        for (ref, pin), net in self.conn.items():
            d[net].append((ref, pin))
        return d

    def pininfo(self, ref, pin):
        t = self.chips[ref]
        lst = PINS[t]
        if pin < 1 or pin > len(lst):
            return None
        return lst[pin - 1]          # (name, dir)

    # -------- rules check --------
    def check(self, sheets=None, quiet=False):
        errs, warns = [], []
        # 1. pins exist
        for (ref, pin), net in self.conn.items():
            if ref not in self.chips:
                errs.append('%s.%d: chip not declared' % (ref, pin))
                continue
            if self.pininfo(ref, pin) is None:
                errs.append('%s.%d (%s): pin does not exist on %s' % (ref, pin, net, self.chips[ref]))
        # 2. supply pins: explicit entries must be on the right rail
        for ref, t in self.chips.items():
            for i, (nm, d) in enumerate(PINS[t], 1):
                key = (ref, i)
                want = None
                if t in PWR and i in PWR[t]:
                    want = PWR[t][i]
                elif d == 'g':
                    want = 'GND'
                elif d == 'v':
                    want = '+5V'
                if want:
                    got = self.conn.get(key)
                    if got is not None and got != want:
                        errs.append('%s.%d (%s) is a %s supply pin but entered on %s' % (ref, i, nm, want, got))
                    self.conn.setdefault(key, want)
        # 3. per-net driver checks
        for net, members in sorted(self.nets().items()):
            outs = [(r, p) for r, p in members if self.pininfo(r, p) and self.pininfo(r, p)[1] == 'o']
            if len(outs) > 1:
                errs.append('net %s has %d outputs: %s' % (net, len(outs), outs))
            ins = [(r, p) for r, p in members if self.pininfo(r, p) and self.pininfo(r, p)[1] == 'i']
            drivers = [m for m in members if self.pininfo(*m) and self.pininfo(*m)[1] in 'otcbx']
            if net not in RAILS and ins and not drivers:
                warns.append('net %s: inputs %s have no driver on the board (connector/off-sheet?)' % (net, ins))
            if net not in RAILS and len(members) == 1:
                warns.append('net %s has a single pin %s' % (net, members[0]))
        # 4. unconnected inputs on declared chips (only for chips with some pins entered)
        for ref, t in self.chips.items():
            entered = [i for i in range(1, len(PINS[t]) + 1) if (ref, i) in self.conn]
            if not entered:
                continue
            for i, (nm, d) in enumerate(PINS[t], 1):
                if d == 'i' and (ref, i) not in self.conn:
                    warns.append('%s.%d (%s) input is not connected' % (ref, i, nm))
        if not quiet:
            for e in errs:
                print('ERROR  ', e)
            for w in warns:
                print('warn   ', w)
            print('%d chips, %d pin connections, %d nets, %d errors, %d warnings' %
                  (len(self.chips), len(self.conn), len(self.nets()), len(errs), len(warns)))
        return errs, warns

    def dump(self, path):
        with open(path, 'w') as f:
            f.write('# Sinclair ZX80 netlist, transcribed from the circuit diagram (see sheet01.py)\n')
            f.write('# chips: ref type\n')
            for ref in sorted(self.chips):
                f.write('chip %s %s\n' % (ref, self.chips[ref]))
            f.write('# nets: name: ref.pin ...\n')
            for net, members in sorted(self.nets().items()):
                ms = ' '.join('%s.%d' % m for m in sorted(members))
                f.write('net %s: %s\n' % (net, ms))


N = Netlist()
