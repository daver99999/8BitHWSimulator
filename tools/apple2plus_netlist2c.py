#!/usr/bin/env python3
"""apple2plus_netlist2c.py - turn the netlist traced from the Apple II+ schematic (tools/apple2plus-netlist-src/apple2plus.net,
made by apple2_sch2net.py) into the board table src/apple2plus_board_data.c (bd_apple2plus). Same format as the other boards'.
The Apple II+ motherboard names its chips by their place on the board: a letter for the row (A at the top) and the column
(1 at the left), so the ICs are put on that grid; the other parts go in rows below the board.
Usage: tools/apple2plus_netlist2c.py [outdir]  (default: src)
"""
import os, re, sys
here = os.path.dirname(os.path.abspath(__file__))

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

def is_ic(typ):
    return typ.startswith('74LS') or typ in ('8T97', 'DP8304', 'NE558', '555', '555_alt1', 'LM741', '4116', 'MOS6502', 'ROM_Chip', '2316_2716')

def footprint(ref, typ, n):
    if typ in ('R', 'R_Small', 'R_Potentiometer_Trim_US'): return 'R_Axial'
    if typ.startswith('R_Network'): return 'R_Array_SIP%d' % n
    if typ in ('C_Small', 'C_Trim_Small'): return 'C_Disc'
    if typ in ('D',): return 'D_DO-35'
    if typ == 'L': return 'L_Axial'
    if typ == 'Crystal': return 'XTAL'
    if typ.startswith('2N') or typ.startswith('Q_'): return 'TO-92'
    if typ.startswith('Solder') or typ.startswith('Jumper'): return 'SolderJumper'
    if is_ic(typ): return 'DIP-%d' % n
    return 'CONN'

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


# --- the part table of the machine (src/apple2plus_parts.inc): type -> (PT, label, role, text, what) ---
TYPES = {
    '74LS00': ('PT_74LS00', 'ROLE_GLUE'), '74LS02': ('PT_74LS02', 'ROLE_GLUE'), '74LS04': ('PT_74LS04', 'ROLE_GLUE'),
    '74LS08': ('PT_74LS08', 'ROLE_GLUE'), '74LS11': ('PT_74LS11', 'ROLE_GLUE'), '74LS20': ('PT_74LS20', 'ROLE_GLUE'),
    '74LS32': ('PT_74LS32', 'ROLE_GLUE'), '74LS51': ('PT_74LS51', 'ROLE_GLUE'), '74LS74': ('PT_74LS74R', 'ROLE_GLUE'),
    '74LS86': ('PT_74LS86', 'ROLE_GLUE'), '74LS138': ('PT_74LS138', 'ROLE_GLUE'), '74LS139': ('PT_74LS139', 'ROLE_GLUE'),
    '74LS151': ('PT_74LS151', 'ROLE_GLUE'), '74LS153': ('PT_74LS153', 'ROLE_GLUE'), '74LS161': ('PT_74LS161', 'ROLE_GLUE'),
    '74LS166': ('PT_74166', 'ROLE_GLUE'), '74LS174': ('PT_74LS174', 'ROLE_GLUE'), '74LS175': ('PT_74LS175', 'ROLE_GLUE'),
    '74LS194': ('PT_74LS194', 'ROLE_GLUE'), '74LS195': ('PT_74LS195', 'ROLE_GLUE'), '74LS251': ('PT_74LS251', 'ROLE_GLUE'),
    '74LS257': ('PT_74LS257', 'ROLE_GLUE'), '74LS259': ('PT_74LS259', 'ROLE_GLUE'), '74LS283': ('PT_74LS283', 'ROLE_GLUE'),
    '8T97': ('PT_8T97', 'ROLE_GLUE'), 'DP8304': ('PT_DP8304', 'ROLE_GLUE'), 'NE558': ('PT_NE558', 'ROLE_CHIP'),
    '4116': ('PT_4116', 'ROLE_RAM'), 'ROM_Chip': ('PT_A2ROM', 'ROLE_ROM'), '2316_2716': ('PT_A2CHR', 'ROLE_ROM'),
    'MOS6502': ('PT_PET6502', 'ROLE_CHIP'),
}
WHAT = {
    'A3': '74LS166 shift register: the dots of a character (from the character generator) one by one to the video output',
    'A5': 'character generator ROM (2316 type): the dots of the characters',
    'A7': 'keyboard connector (the key encoder is modelled here: seven data lines and the strobe)',
    'A15': '555 power-on reset timer: holds RESET low for a while',
    'B3': '555 oscillator: the flashing of the characters (about 2 Hz)',
    'Y1': '14.31818 MHz crystal (modelled as a clock source): the board divides it for everything',
    'Q5': 'transistor: the 555 holds /RESET low through it',
    'U3': '6502 CPU, 1.02 MHz',
    'C1': '74LS153: the 7th row/column address bit of the 4116s (CPU A13 / A12, or the video address)',
    'E11': '74LS153 RAM address multiplexer: RA0 and RA3 (CPU or video address, row or column)',
    'E12': '74LS153 RAM address multiplexer: RA1 and RA4 (CPU or video address, row or column)',
    'E13': '74LS153 RAM address multiplexer: RA2 and RA5 (CPU or video address, row or column)',
    'F2': '74LS139: the CAS strobe of the 16K bank (CCAS, DCAS, ECAS)',
    'F14': '74LS259 soft switch latch: text/graphics, mixed, page 2, hi-res and the game connector annunciators',
    'H3': '8T97 buffer: the CPU address lines to the bus', 'H4': '8T97 buffer: the CPU address lines to the bus',
    'H5': '8T97 buffer: the CPU address lines and R/W to the bus',
    'H10': 'DP8304 data bus buffer between the CPU and the bus',
    'H13': 'NE558 timers of the game connector (no paddles fitted: PDL reads 255)',
    'H14': '74LS251: game connector and cassette inputs onto data bit 7',
    'E2': 'spare 74LS139 (none of its inputs is drawn on the schematic)',
}
LABEL = {'4116': 'RAM', 'ROM_Chip': 'ROM', '2316_2716': 'CHAR'}
def partdefs(chips):
    out = ['/* apple2plus_parts.inc - GENERATED by tools/apple2plus_netlist2c.py, do not edit: the chips of the Apple II+ and their models. */']
    def key(ref): return sortkey(ref)
    for ref in sorted(chips, key=key):
        typ, val, n = chips[ref]
        m = None
        if ref == 'Y1': m = ('PT_XTAL14', 'ROLE_CLOCK', '14.31818', 'crystal')
        elif ref == 'A7': m = ('PT_A2KBD', 'ROLE_CHIP', 'KBD', 'keyboard')
        elif ref == 'A15': m = ('PT_LM555', 'ROLE_CLOCK', 'LM555', 'reset')
        elif ref == 'B3': m = ('PT_A2FLASH', 'ROLE_CLOCK', '555', 'flash')
        elif ref == 'Q5': m = ('PT_A2NPN', 'ROLE_CLOCK', '2N3904', 'reset')
        elif typ in TYPES:
            pt, role = TYPES[typ]
            lab = LABEL.get(typ, typ)
            text = None
            if typ == '4116':
                bank = {'C': '$0000-$3FFF', 'D': '$4000-$7FFF', 'E': '$8000-$BFFF'}[ref[0]]
                text = '"4116\\n%s\\nbit %d"' % (bank, int(ref[1:]) - 3)
            elif typ == 'ROM_Chip': text = '"2316\\n$%s"' % ref[4:].ljust(2, '0') + '00'.replace('00', '')
            m = (pt, role, lab, text)
        if not m: continue
        pt, role, lab = m[0], m[1], m[2]
        text = m[3] if len(m) > 3 else None
        if typ == 'ROM_Chip':
            text = '"2316\\n$%s00"' % ref[4:]
        elif isinstance(text, str) and not text.startswith('"') and text:
            text = '"%s"' % text
        what = WHAT.get(ref)
        if typ == '4116': what = '4116 DRAM, 16K x 1: one bit of ' + {'C': '$0000-$3FFF', 'D': '$4000-$7FFF', 'E': '$8000-$BFFF'}[ref[0]]
        if typ == 'ROM_Chip': what = '2K ROM at $%s00' % ref[4:]
        if not what: what = typ + ' (' + (val or '') + ')'
        out.append('    { "%s", &%s, "%s", %s, %s, "%s" },' % (ref, pt, lab.replace('"', ''), role, text if text else 'NULL', what.replace('"', "'")))
    return '\n'.join(out) + '\n'

def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, '..', 'src')
    chips, nets = read(os.path.join(here, 'apple2plus-netlist-src', 'apple2plus.net'))
    padnet, pinsof = {}, {}
    for name, members in nets.items():
        if len(members) < 2 and name.startswith('N$'): continue      # a stub, not a net
        for m in members:
            ref, pin = m.rsplit('.', 1)
            padnet[(ref, pin)] = name
            pinsof.setdefault(ref, set()).add(pin)
    # the netlist's nets that are named but have a single pad stay (they are test points or unused outputs)
    used = sorted({n for n in padnet.values()})
    netidx = {n: i for i, n in enumerate(used)}
    W, H = 330.0, 250.0
    placed = {}
    loose = []
    ics = {}
    for ref, (typ, val, n) in chips.items():
        names = pad_names(typ, n, pinsof.get(ref, set()))
        fp = footprint(ref, typ, n)
        m = re.fullmatch(r'([A-K])(\d{1,2})', ref)
        if m and fp.startswith('DIP') and n <= 20 and int(m.group(2)) <= 14:
            row, col = ord(m.group(1)) - ord('A'), int(m.group(2)) - 1
            x0 = 14 + col * 22.0
            y0 = 12 + row * 22.0
            placed[ref] = pads_for(fp, names, x0, y0)[0]
        else:
            loose.append((ref, fp, names))
    x, y, rowh = 10.0, H + 12, 0.0
    for ref, fp, names in sorted(loose, key=lambda c: (c[1] != 'DIP-24' and c[1] != 'DIP-40', c[1], sortkey(c[0]))):
        pads, (w, h) = pads_for(fp, names, x, y)
        if x + w > W:
            x, y, rowh = 10.0, y + rowh + 8, 0.0
            pads, (w, h) = pads_for(fp, names, x, y)
        placed[ref] = pads
        x += w + 6
        rowh = max(rowh, h)
    height = y + rowh + 10
    out = ['/* apple2plus_board_data.c - GENERATED by tools/apple2plus_netlist2c.py, do not edit.\n * Apple II+ (rev 0 board, schematic from the Apple II Reference Manual of 1979 redrawn in KiCad by baldengineer, "bit-preserve"):\n'
           ' * the netlist is traced from the sheets by tools/apple2_sch2net.py; the ICs sit on the board\'s row/column grid (B5 = row B, column 5), the rest in rows below. */\n']
    out.append('#include "board_data.h"\n\nstatic const char *const nets[] = {')
    for n in used: out.append('    "%s",' % n.replace('\\', '\\\\').replace('"', '\\"'))
    out.append('};\n\nstatic const bd_pad_t pads[] = {')
    first, comps = {}, []
    k = 0
    for ref in sorted(chips, key=sortkey):
        typ, val, n = chips[ref]
        first[ref] = k
        for (nm, px, py, w, h) in placed[ref]:
            net = padnet.get((ref, nm))
            out.append('    { "%s", %.2ff, %.2ff, %.2ff, %.2ff, %d },' % (nm, px, py, w, h, netidx[net] if net else -1))
            k += 1
        comps.append((ref, typ, val, len(placed[ref])))
    out.append('};\n\nstatic const bd_comp_t comps[] = {')
    for ref, typ, val, n in comps:
        out.append('    { "%s", "%s", "%s", %d, %d },' % (ref, val.replace('"', "'") if val else typ, footprint(ref, typ, n), first[ref], n))
    out.append('};\n')
    out.append('const board_data_t bd_apple2plus = {\n    { 0, 0, %.0f, %.0f },\n    nets, %d, pads, %d, comps, %d };\n' % (W, height, len(used), k, len(comps)))
    open(os.path.join(outdir, 'apple2plus_parts.inc'), 'w').write(partdefs(chips))
    path = os.path.join(outdir, 'apple2plus_board_data.c')
    open(path, 'w').write('\n'.join(out))
    print(path, len(comps), 'parts', len(used), 'nets', k, 'pads')

if __name__ == '__main__':
    main()
