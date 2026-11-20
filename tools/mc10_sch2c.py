#!/usr/bin/env python3
"""mc10_sch2c.py - turn the Eagle schematic of the Tandy MC-10 (the "MC-10 Schematics.sch" of the MC-10 repository with the A/V mod
board) into the board table src/mc10_board_data.c (const board_data_t bd_mc10) and the part table src/mc10_parts.inc.

The schematic has no board layout (the repository's .brd files are the A/V modification board), so the parts are placed in rows
by reference; their pads are numbered as the schematic's libraries number them (DIP packages). Only the digital part of the
machine is taken: the CPU, the video chip, the memories, the logic, the keyboard connectors, the reset switch, the expansion
connector and the pull-up resistors. The video encoder (MC1372), the cassette comparators, the power supply, the capacitors and
the diodes are analog and left out; the MC1372's oscillator is a clock source (IC2).

Usage: tools/mc10_sch2c.py "<MC-10 Schematics.sch>" [outdir]   (outdir default: src)
"""
import os, re, sys
import xml.etree.ElementTree as ET

here = os.path.dirname(os.path.abspath(__file__))

# the parts that are taken: ref -> (model, label, role, info, detail), None for a part that is only wiring
MODELS = {
    'U1': ('PT_6803', '6803', 'ROLE_CHIP', 'cpu', 'MC6803 CPU with its own 128 bytes of RAM, timer and ports: EXTAL 3.58 MHz, E 0.89 MHz'),
    'U2': ('PT_74LS373', '74LS373', 'ROLE_GLUE', None, '74LS373: holds the low byte of the address (the 6803 multiplexes it with the data, AS latches it)'),
    'U3': ('PT_TMS2564', 'ROM', 'ROLE_ROM', 'ROM', 'TMS2564 ROM, 8K: Microcolor BASIC ($E000-$FFFF)'),
    'U4': ('PT_74LS155', '74LS155', 'ROLE_GLUE', None, '74LS155: the chip selects from A14, A15 (the video RAM, the ROM, the keyboard and the video latch)'),
    'U5': ('PT_74LS367', '74LS367', 'ROLE_GLUE', None, '74LS367: the CPU\'s address for the video RAM (while the CPU uses it)'),
    'U6': ('PT_74LS367', '74LS367', 'ROLE_GLUE', None, '74LS367: the CPU\'s address for the video RAM (while the CPU uses it)'),
    'U7': ('PT_74LS245', '74LS245', 'ROLE_GLUE', None, '74LS245: the CPU\'s data bus to the video RAM'),
    'U8': ('PT_74LS174', '74LS174', 'ROLE_GLUE', None, '74LS174: the video modes (GM0-GM2, A/G, CSS) and the sound bit, written at $BFFF'),
    'U9': ('PT_6116', 'RAM', 'ROLE_RAM', '4016\n2K x 8', 'uPD4016 SRAM, 2K x 8: the first 2K of the RAM (the screen and BASIC\'s variables)'),
    'U10': ('PT_6116', 'RAM', 'ROLE_RAM', '4016\n2K x 8', 'uPD4016 SRAM, 2K x 8: the second 2K of the RAM'),
    'U11': ('PT_6847_MC10', 'VDG', 'ROLE_CHIP', 'vdg', '6847 video chip: reads the screen from the RAM itself while the CPU is not using it'),
    'U12': ('PT_74LS14', '74LS14', 'ROLE_GLUE', None, '74LS14: inverters (reset, the chip select, the oscillator)'),
    'U13': ('PT_74LS32', '74LS32', 'ROLE_GLUE', None, '74LS32: the RAM write, the video clock, the keyboard read and the video latch strobe'),
    'U14': ('PT_MC14503', '14503', 'ROLE_GLUE', None, 'MC14503B: puts the keyboard columns on the data bus when $BFFF is read'),
    'U18': ('PT_74LS74F', '74LS74', 'ROLE_GLUE', None, '74LS74: the window for the CPU\'s video RAM writes and the video clock stretch'),
    'U19': ('PT_74LS76', '74LS76', 'ROLE_GLUE', None, '74LS76: the video clock stretch, started by the horizontal sync'),
    'U20': ('PT_74LS32', '74LS32', 'ROLE_GLUE', None, '74LS32: one gate: the second decoder\'s enable'),
    'IC2': ('PT_XTAL_MC10', 'XTAL', 'ROLE_CLOCK', 'xtal', 'MC1372 video modulator: its oscillator runs the 3.579545 MHz crystal (the colour and RF parts are not simulated)'),
}
# Mistakes of the schematic: the 6803's A12 (pin 25) is drawn on the net of A11 (pin 26 is too), which leaves the net A12 (the ROM,
# the decoder's gate U20 and the expansion connector) without a driver. The pin is A12 on the real board.
PATCH = {('U1', '25'): 'A12'}
PASSIVE = ('R21', 'R22', 'R23', 'R24', 'R25', 'R26', 'RN1', 'C8')
OTHER = ('X101', 'S1', 'J4', 'J5', 'E.')


def load(path):
    r = ET.parse(path).getroot()
    pads = {}
    for lib in r.findall('.//libraries/library'):
        for ds in lib.findall('.//devicesets/deviceset'):
            for dev in ds.findall('devices/device'):
                m = {}
                for c in dev.findall('connects/connect'):
                    m[(c.get('gate'), c.get('pin'))] = c.get('pad')
                pads[(lib.get('name'), ds.get('name'), dev.get('name'))] = m
    parts = {}
    for p in r.findall('.//schematic/parts/part'):
        parts[p.get('name')] = (p.get('library'), p.get('deviceset'), p.get('device'), p.get('value'))
    nets = {}
    for sh in r.findall('.//schematic/sheets/sheet'):
        for n in sh.findall('nets/net'):
            for seg in n.findall('segment'):
                for pr in seg.findall('pinref'):
                    part = pr.get('part')
                    lib, ds, dev, val = parts[part]
                    pad = pads.get((lib, ds, dev), {}).get((pr.get('gate'), pr.get('pin')))
                    if pad is None: continue
                    nets.setdefault(n.get('name'), set()).add((part, pad))
    return parts, nets, pads


def dip(n):
    """pads of a DIP with n pins, pin 1 at the top left: (number, x, y) relative to the top left"""
    wr = 15.24 if n >= 24 else 7.62
    half = n // 2
    out = []
    for i in range(1, n + 1):
        if i <= half: out.append((i, 0.0, (i - 1) * 2.54))
        else: out.append((i, wr, (n - i) * 2.54))
    return out, wr, (half - 1) * 2.54


def cstr(s): return s.replace('\\', '\\\\').replace('"', '\\"')


def main():
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(1)
    outdir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(here, '..', 'src')
    parts, nets, _ = load(sys.argv[1])
    take = set(MODELS) | set(PASSIVE) | set(OTHER)
    padnet = {}
    for name, members in nets.items():
        for part, pad in members:
            if part in take:
                padnet[(part, pad)] = name
    padnet.update(PATCH)
    netnames = sorted({n for n in padnet.values() if sum(1 for k, v in padnet.items() if v == n) > 0})
    idx = {n: i for i, n in enumerate(netnames)}

    def refkey(ref):
        m = re.match(r'([A-Za-z.]+)(\d*)', ref)
        return (m.group(1), int(m.group(2) or 0))

    comps = []
    for ref in sorted(take, key=refkey):
        if ref not in parts: continue
        names = sorted({pad for (r, pad) in padnet if r == ref}, key=lambda s: (len(s), s))
        numeric = [int(x) for x in names if x.isdigit()]
        if ref in MODELS or ref in ('J4', 'J5', 'E.', 'S1'):
            n = max(numeric) if numeric else len(names)
            if ref in ('U2', 'U7'): n = 20
            if ref == 'U12' or ref == 'U13' or ref == 'U20' or ref == 'U18': n = 14
            if ref in ('U19',): n = 16
        else:
            n = 2
        comps.append((ref, names, n))

    # layout: rows of parts by reference, ICs first
    out_pads, out_comps = [], []
    x, y, rowh = 6.0, 6.0, 0.0
    maxw = 300.0
    order = [c for c in comps if c[0] in MODELS] + [c for c in comps if c[0] not in MODELS]
    placed = {}
    for ref, names, n in order:
        dipped = ref in MODELS or ref == 'E.'
        if ref in ('J4', 'J5', 'S1', 'X101'): dipped = False
        if dipped:
            pl, w, h = dip(n)
            w += 4; h += 4
            if ref == 'E.':
                pl = [(i, ((i - 1) % 2) * 2.54, ((i - 1) // 2) * 2.54) for i in range(1, n + 1)]
                w, h = 8, ((n + 1) // 2) * 2.54 + 2
            fp = 'DIP-%d' % n if ref != 'E.' else 'CONN'
        else:
            nn = n if ref in ('J4', 'J5') else 2
            if ref in ('J4', 'J5'): nn = 8
            if ref == 'RN1': nn = 8
            pl = [(i, (i - 1) * 2.54 if ref in ('J4', 'J5', 'RN1') else (i - 1) * 7.62, 0.0) for i in range(1, nn + 1)]
            w, h = max(p[1] for p in pl) + 4, 4
            fp = 'R_Array_SIP%d' % nn if ref == 'RN1' else 'CONN' if ref in ('J4', 'J5') else 'R_Axial'
            if ref == 'C8': fp = 'C_Disc'
            if ref == 'X101': fp = 'XTAL'
            if ref == 'S1': fp = 'SW'
            if ref == 'IC2':
                pass
        if x + w > maxw:
            x, y, rowh = 6.0, y + rowh + 6, 0.0
        placed[ref] = (x, y, pl, fp)
        x += w + 4
        rowh = max(rowh, h)
    H = y + rowh + 6
    k = 0
    for ref, names, n in sorted(order, key=lambda c: refkey(c[0])):
        px, py, pl, fp = placed[ref]
        first = k
        # eagle pad names -> pin numbers: digits stay, others by order ('+' then '-')
        namemap = {}
        extra = [nm for nm in names if not nm.isdigit()]
        for nm in names:
            namemap[nm] = int(nm) if nm.isdigit() else len(names) - len(extra) + extra.index(nm) + 1
            if ref == 'C8': namemap[nm] = 1 if nm == '+' else 2
        pinnet = {namemap[nm]: padnet.get((ref, nm)) for nm in names}
        for (i, dx, dy) in pl:
            net = pinnet.get(i)
            out_pads.append('    { "%d", %.2ff, %.2ff, 1.60f, 1.60f, %d },' % (i, px + dx + 2, py + dy + 2, idx[net] if net else -1))
            k += 1
        out_comps.append('    { "%s", "%s", "%s", %d, %d },' % (cstr(ref), cstr(parts[ref][3] or parts[ref][1]), fp, first, k - first))
    text = ['/* mc10_board_data.c - GENERATED by tools/mc10_sch2c.py, do not edit.\n * Tandy MC-10 (schematic): the digital part of the Eagle schematic of the MC-10 repository; the schematic has no layout,\n * the parts are placed in rows. */\n',
            '#include "board_data.h"\n\nstatic const char *const nets[] = {']
    text += ['    "%s",' % cstr(n) for n in netnames]
    text += ['};\n\nstatic const bd_pad_t pads[] = {'] + out_pads + ['};\n\nstatic const bd_comp_t comps[] = '] + ['{'] + out_comps + ['};\n']
    text.append('const board_data_t bd_mc10 = {\n    { 0, 0, %.1ff, %.1ff },\n    nets, %d, pads, %d, comps, %d };\n' % (maxw, H, len(netnames), k, len(out_comps)))
    open(os.path.join(outdir, 'mc10_board_data.c'), 'w').write('\n'.join(text))
    inc = ['/* mc10_parts.inc - GENERATED by tools/mc10_sch2c.py, do not edit: the chips and their models. */']
    for ref in sorted(MODELS, key=refkey):
        m = MODELS[ref]
        info = 'NULL' if m[3] is None else '"%s"' % m[3].replace('\n', '\\n')
        inc.append('    { "%s", &%s, "%s", %s, %s, "%s" },' % (ref, m[0], m[1], m[2], info, cstr(m[4])))
    open(os.path.join(outdir, 'mc10_parts.inc'), 'w').write('\n'.join(inc) + '\n')
    print('mc10_board_data.c', len(out_comps), 'parts', len(netnames), 'nets', k, 'pads;', 'board %.0f x %.0f mm' % (maxw, H))


if __name__ == '__main__':
    main()
