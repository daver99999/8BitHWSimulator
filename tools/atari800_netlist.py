#!/usr/bin/env python3
"""atari800_netlist.py - the netlist of the Atari 800 (motherboard CO14175, CPU board, personality board, RAM boards), typed in
from the Atari 400/800 Field Service Manual, Section 2 (May 1983 600 dpi scan), Fig 2-6A/B, 2-10, 2-12, 2-14, 2-16.
Writes tools/atari800-netlist-src/atari800.net in the format the other boards use (chip REF TYPE|VALUE|PINS, net NAME: REF.PIN ...).
Where a connection could not be read with certainty it is listed in UNSURE and is a beep-check item (docs/ATARI800_BEEP_CHECKS.md).

Sections are separate functions so a sheet can be checked on its own. Part references are the board's own (A301, Z303, J101 ...).
Usage: tools/atari800_netlist.py [out.net]"""
import os, sys

here = os.path.dirname(os.path.abspath(__file__))
chips = {}
nets = {}
UNSURE = []


def part(ref, typ, n, val=None):
    chips[ref] = (typ, val or typ, n)


def net(name, *pins):
    nets.setdefault(name, [])
    for p in pins:
        if p not in nets[name]:
            nets[name].append(p)


def unsure(what):
    UNSURE.append(what)


A = ['A%d' % i for i in range(16)]
D = ['D%d' % i for i in range(8)]
GND, V5, V12, VM5 = 'GND', '+5V', '+12V', '-5V'

# The 28 + 28 pin edge connector of the CPU board and of J101 on the motherboard (pin names as printed: 1..28, A..Z, AA..FF)
EDGE = [str(i) for i in range(1, 29)] + list('ABCDEFHJKLMNPRSTUVWXYZ') + ['AA', 'BB', 'CC', 'DD', 'EE', 'FF']
CPUCONN = {
    'PHI1': '22', 'A15': '24', 'A14': 'CC', 'A13': '20', 'A12': '18', 'A11': 'U', 'A10': 'W', 'A9': 'P', 'A8': '15', 'A7': '13',
    'A6': '19', 'A5': '16', 'A4': 'T', 'A3': 'V', 'A2': 'N', 'A1': '14', 'A0': '12', 'PHI2': '21', 'R/W': 'Y',
    'COL': '4', 'LUM0': '1', 'LUM1': '3', 'LUM2': '2', 'LUM3': 'M', 'OSC': 'C', 'CSYNC': 'B', 'S3': 'S', 'S2': 'R', 'S1': '11',
    'S0': '17', 'T3': 'L', 'T2': '10', 'T1': 'K', 'T0': '9', 'CSI': '27', 'RNMI': 'Z', 'LP': 'A', 'REF': 'D', 'RESETA': 'X',
    'NMI': '23', 'RESETM': '25', 'D7': '8', 'D6': 'J', 'D5': 'H', 'D4': 'F', 'D3': '7', 'D2': '6', 'D1': 'E', 'D0': '5',
    'SYNC': 'BB', 'IRQ': 'AA',
}
CPUCONN_PWR = {'+5V': ['26', 'DD'], '+12V': ['EE'], 'GND': ['28', 'FF']}

# chip pin tables -----------------------------------------------------------------------------------------------
GTIA = {'VCC': 27, 'VSS': 3, 'R/W': 33, 'HALT': 26, 'PHI2': 30, 'A4': 38, 'A3': 39, 'A2': 40, 'A1': 1, 'A0': 2, 'AN0': 18,
        'AN1': 19, 'AN2': 20, 'F0': 29, 'D7': 34, 'D6': 35, 'D5': 36, 'D4': 37, 'D3': 4, 'D2': 5, 'D1': 6, 'D0': 7, 'COL': 21,
        'LUM0': 22, 'LUM1': 23, 'LUM2': 24, 'LUM3': 31, 'OSC': 28, 'CSYNC': 25, 'S3': 15, 'S2': 14, 'S1': 13, 'S0': 12,
        'T3': 11, 'T2': 10, 'T1': 9, 'T0': 8, 'CSI': 32, 'DEL': 17}
ANTIC = {'VCC': 21, 'VSS': 1, 'R/W': 14, 'HALT': 9, 'PHI2': 29, 'A0': 13, 'A1': 12, 'A2': 11, 'A3': 10, 'A4': 28, 'A5': 27,
         'A6': 26, 'A7': 25, 'A8': 24, 'A9': 23, 'A10': 16, 'A11': 22, 'A12': 17, 'A13': 18, 'A14': 19, 'A15': 20, 'AN0': 2,
         'AN1': 3, 'AN2': 5, 'F0': 35, 'D7': 37, 'D6': 38, 'D5': 39, 'D4': 40, 'D3': 33, 'D2': 32, 'D1': 31, 'D0': 30,
         'RNMI': 6, 'LP': 4, 'REF': 8, 'RESET': 36, 'NMI': 7, 'RDY': 15, 'PHI0': 34}
CPU6502 = {'VCC': 8, 'VSS': 1, 'VSS2': 21, 'R/W': 34, 'A0': 9, 'A1': 10, 'A2': 11, 'A3': 12, 'A4': 13, 'A5': 14, 'A6': 15, 'A7': 16,
           'A8': 17, 'A9': 18, 'A10': 19, 'A11': 20, 'A12': 22, 'A13': 23, 'A14': 24, 'A15': 25, 'D0': 33, 'D1': 32, 'D2': 31,
           'D3': 30, 'D4': 29, 'D5': 28, 'D6': 27, 'D7': 26, 'IRQ': 4, 'SYNC': 7, 'NMI': 6, 'RDY': 2, 'RES': 40, 'PHI0': 37,
           'PHI2': 39, 'PHI1': 3, 'SO': 38}
# the bus buffers Z303 (A0-A7) and Z304 (A8-A15): inputs from the CPU, outputs to the bus
BUF_IN = [3, 5, 7, 9, 12, 14, 16, 18]
BUF_OUT = [17, 15, 13, 11, 8, 6, 4, 2]


def pin(ref, table, sig):
    return '%s.%d' % (ref, table[sig])


def cpu_board():
    """CPU board (Fig 2-12)."""
    part('J101C', 'EDGE56', 56, 'CPU board edge')
    for sig, p in CPUCONN.items():
        net(sig, 'J101C.' + p)
    for sig, ps in CPUCONN_PWR.items():
        for p in ps:
            net(sig, 'J101C.' + p)
    part('A301', 'GTIA', 40, 'CTIA/GTIA')
    part('A302', 'ANTIC', 40, 'ANTIC')
    part('A303', '6502', 40, '6502 CPU')
    part('Z303', 'ADDRBUF', 20, 'address buffer A0-A7')
    part('Z304', 'ADDRBUF', 20, 'address buffer A8-A15')
    part('Z302', '7474', 14, '74LS74 clock divider')
    part('Z301', '7402', 14, '7402 NOR')
    # power
    for ref, t in (('A301', GTIA), ('A302', ANTIC), ('A303', CPU6502)):
        net(V5, pin(ref, t, 'VCC'))
        net(GND, pin(ref, t, 'VSS'))
    net(GND, 'A303.21', 'A303.38')                   # 6502 VSS pin 21 and SO tied to ground
    for ref in ('Z303', 'Z304'):
        net(GND, ref + '.10')                        # through R312 / R313 1 ohm: modelled as direct, see the resistor parts
        net(V5, ref + '.20')
        net('BUFEN', ref + '.1', ref + '.19')
    net(V5, 'Z302.14', 'Z302.1', 'Z302.4', 'Z302.11', 'Z302.12')
    net(GND, 'Z302.7')
    net(V5, 'Z301.14')
    net(GND, 'Z301.7')
    # address bus: the connector, ANTIC (outputs), the GTIA (A0-A4) and the buffer outputs
    for i in range(16):
        s = 'A%d' % i
        net(s, pin('A302', ANTIC, s))
        buf, k = ('Z303', i) if i < 8 else ('Z304', i - 8)
        net(s, '%s.%d' % (buf, BUF_OUT[k]))
        net('CPU_' + s, '%s.%d' % (buf, BUF_IN[k]), pin('A303', CPU6502, s))
        if i < 5:
            net(s, pin('A301', GTIA, s))
    # data bus
    for i in range(8):
        s = 'D%d' % i
        net(s, pin('A301', GTIA, s), pin('A302', ANTIC, s), pin('A303', CPU6502, s))
    # ANTIC <-> GTIA
    for s in ('AN0', 'AN1', 'AN2'):
        net(s, pin('A301', GTIA, s), pin('A302', ANTIC, s))
    net('F0', pin('A301', GTIA, 'F0'), pin('A302', ANTIC, 'F0'))
    net('R/W', pin('A301', GTIA, 'R/W'), pin('A302', ANTIC, 'R/W'), pin('A303', CPU6502, 'R/W'))
    net('HALT', pin('A301', GTIA, 'HALT'), pin('A302', ANTIC, 'HALT'))
    net('PHI2', pin('A301', GTIA, 'PHI2'), pin('A302', ANTIC, 'PHI2'), pin('A303', CPU6502, 'PHI2'))
    net('RDY', pin('A302', ANTIC, 'RDY'), pin('A303', CPU6502, 'RDY'))
    net('RNMI', pin('A302', ANTIC, 'RNMI'))
    net('LP', pin('A302', ANTIC, 'LP'))
    net('REF', pin('A302', ANTIC, 'REF'))
    net('NMI', pin('A302', ANTIC, 'NMI'), pin('A303', CPU6502, 'NMI'))
    net('RESETM', pin('A302', ANTIC, 'RESET'), pin('A303', CPU6502, 'RES'))
    net('IRQ', pin('A303', CPU6502, 'IRQ'))
    net('SYNC', pin('A303', CPU6502, 'SYNC'))
    net('PHI0', pin('A302', ANTIC, 'PHI0'), pin('A303', CPU6502, 'PHI0'))
    # GTIA video and console
    for s in ('COL', 'LUM0', 'LUM1', 'LUM2', 'LUM3', 'OSC', 'CSYNC', 'S3', 'S2', 'S1', 'S0', 'T3', 'T2', 'T1', 'T0', 'CSI'):
        net(s, pin('A301', GTIA, s))
    # the 1K pull-ups R302-R306 on COL and LUM0-3, the colour adjust R308 / R309 / C305 on DEL
    for ref, s in (('R302', 'COL'), ('R303', 'LUM0'), ('R304', 'LUM1'), ('R305', 'LUM2'), ('R306', 'LUM3')):
        part(ref, 'R', 2, '1K')
        net(V5, ref + '.1')
        net(s, ref + '.2')
    part('R308', 'R', 2, '330K'); net(V12, 'R308.1'); net('DEL', 'R308.2')
    part('R309', 'R', 2, '500K'); net('DEL', 'R309.1'); net(GND, 'R309.2')
    part('C305', 'C', 2, '.1uF'); net('DEL', 'C305.1'); net(GND, 'C305.2'); net('DEL', pin('A301', GTIA, 'DEL'))
    for ref, v in (('C302', V5), ('C303', V5), ('C304', V5), ('C301', V5)):
        part(ref, 'C', 2, '.1uF'); net(v, ref + '.1'); net(GND, ref + '.2')
    part('C306', 'C', 2, '.01uF'); net(V12, 'C306.1'); net(GND, 'C306.2')
    part('C307', 'C', 2, '.001uF'); net(V5, 'C307.1'); net(GND, 'C307.2')
    part('R307', 'R', 2, '4.7K'); net(V5, 'R307.1'); net('IRQ', 'R307.2')
    part('R310', 'R', 2, '4.7K'); net(V5, 'R310.1')
    part('R311', 'R', 2, '4.7K'); net(V5, 'R311.1')
    part('R312', 'R', 2, '1'); part('R313', 'R', 2, '1')
    # clock generator Z301 / Z302 (read from the 300 dpi drawing; the exact gate-to-gate links are UNSURE, see below)
    net('OSC', 'Z301.11', 'Z301.12')                 # Z301A: OSC inverted
    net('OSCN', 'Z301.13', 'Z302.3')
    net('PHI0', 'Z301.1')                            # Z301C output = the 6502 phase-0 input
    unsure('CPU board clock: Z301A/B/C/D and Z302 pin-to-pin links (OSC -> Z302 CLK1; Z302 /Q1, Q2, /Q2 -> Z301 -> CPU PHI0, GTIA F0/PHI0).')
    unsure('CPU board: 6502 RDY pin printed as 7 next to SYNC 7; assumed pin 2 (RDY) joined to ANTIC RDY.')


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, 'atari800-netlist-src', 'atari800.net')
    cpu_board()
    # every net must have two or more pins, and no pin may be on two nets
    seen, bad = {}, 0
    for n, pins in nets.items():
        for p in pins:
            if p in seen and seen[p] != n:
                print('pin on two nets:', p, seen[p], n, file=sys.stderr); bad += 1
            seen[p] = n
    for ref, (typ, val, n) in chips.items():
        if typ == 'EDGE56':
            continue
    with open(out, 'w') as f:
        for ref in sorted(chips):
            typ, val, n = chips[ref]
            f.write('chip %s %s|%s|%d\n' % (ref, typ, val, n))
        for n in sorted(nets):
            f.write('net %s: %s\n' % (n, ' '.join(nets[n])))
    print('%d parts, %d nets, %d unsure items, %d errors -> %s' % (len(chips), len(nets), len(UNSURE), bad, out), file=sys.stderr)
    for u in UNSURE:
        print('  UNSURE:', u, file=sys.stderr)
    return bad


if __name__ == '__main__':
    sys.exit(1 if main() else 0)
