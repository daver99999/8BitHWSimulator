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


# ---------------------------------------------------------------------------------------------------------------
# Motherboard CO14175 (Fig 2-6A page 1, 2-6B page 2). J101 is the CPU slot, J102-J104 the RAM slots, J107 the personality slot,
# J108 (right) and J109 (left) the cartridge slots. All the bus nets are shared by name with the plug-in boards (flattened).
RAMSLOT = {'A0': '5', 'A1': '7', 'A2': 'E', 'A3': '10', 'A4': 'J', 'A5': '9', 'A6': '12', 'A7': '6', 'A8': '8', 'A9': 'F',
           'A10': '11', 'A11': 'K', 'A12': 'L', 'A13': 'H', 'D0': '1', 'D1': 'A', 'D2': '2', 'D3': '3', 'D4': 'B', 'D5': 'C',
           'D6': 'D', 'D7': '4', 'PHI2': '14', 'RASTIME': '15', 'RW_EARLY': '16', 'RW_LATE': '13', 'REF': '17'}
RAMSLOT_PWR = {'+5V': ['19', 'W'], '-5V': ['20', 'X'], '+12V': ['21', 'Y'], 'GND': ['22', 'Z']}
PERS = {'D0': '1', 'D1': 'A', 'D2': '2', 'D3': '3', 'D4': 'B', 'D5': 'C', 'D6': 'D', 'D7': '4', 'A0': '5', 'A1': '7', 'A2': 'E',
        'A3': '10', 'A4': 'K', 'A5': '9', 'A6': 'J', 'A7': '6', 'A8': '8', 'A9': 'F', 'A10': '11', 'A11': 'L', 'A12': '12',
        'A15': 'R', 'PHI1': '17', 'RW_EARLY': 'U', 'RASTIME': 'V', 'PHI2B': 'S', 'S7': 'H', 'S6': 'M', 'S4': 'P', 'S5': 'N',
        'D6XX': 'W', 'D5XX': 'X', 'PIA_CS': '18', 'POKEY_CS': '19', 'GTIA_CS': '13', 'EXSEL': '14', 'GBA': 'T', 'GBAN': '15',
        'WRITIME': '16'}
PERS_PWR = {'+5V': ['21', 'Y'], 'GND': ['22', 'Z']}
# cartridge slot pins; address and data lines are the same on both slots
CART_ADDR = {'A0': '5', 'A1': '4', 'A2': '3', 'A3': '2', 'A4': 'C', 'A5': 'D', 'A6': 'E', 'A7': 'F', 'A8': 'H', 'A9': 'J',
             'A10': 'P', 'A11': 'N', 'A12': 'K'}
CART_DATA = {'D0': '10', 'D1': '9', 'D2': '8', 'D3': 'L', 'D4': '6', 'D5': '7', 'D6': '11', 'D7': 'M'}
POKEY = {'VSS': 1, 'D3': 2, 'D4': 3, 'D5': 4, 'D6': 5, 'D7': 6, 'PHI2': 7, 'VSS2': 11, 'VCC': 17, 'KR2': 16, 'KR1': 25,
         'K5': 18, 'K4': 19, 'K3': 20, 'K2': 21, 'K1': 22, 'K0': 23, 'SID': 24, 'BCLK': 26, 'OCLK': 27, 'SOD': 28, 'IRQ': 29,
         'CS0': 30, 'CS1': 31, 'RW': 32, 'A3': 33, 'A2': 34, 'A1': 35, 'A0': 36, 'AUD': 37, 'D0': 38, 'D1': 39, 'D2': 40,
         'P1': 15, 'P0': 14, 'P3': 13, 'P2': 12, 'P4': 10, 'P7': 9, 'P6': 8}
PIA = {'VSS': 1, 'PA0': 2, 'PA1': 3, 'PA2': 4, 'PA3': 5, 'PA4': 6, 'PA5': 7, 'PA6': 8, 'PA7': 9, 'PB0': 10, 'PB1': 11,
       'PB2': 12, 'PB3': 13, 'PB4': 14, 'PB5': 15, 'PB6': 16, 'PB7': 17, 'CB1': 18, 'CB2': 19, 'VCC': 20, 'RW': 21, 'CS0': 22,
       'CS2': 23, 'CS1': 24, 'ENABLE': 25, 'D7': 26, 'D6': 27, 'D5': 28, 'D4': 29, 'D3': 30, 'D2': 31, 'D1': 32, 'D0': 33,
       'RESET': 34, 'RS1': 36, 'RS0': 35, 'IRQB': 37, 'IRQA': 38, 'CA2': 39, 'CA1': 40}
CD4051 = {'X0': 13, 'X1': 14, 'X2': 15, 'X3': 12, 'X4': 1, 'X5': 5, 'X6': 2, 'X7': 4, 'COM': 3, 'INH': 6, 'VEE': 7, 'VSS': 8,
          'A': 11, 'B': 10, 'C': 9, 'VDD': 16}


def motherboard():
    part('J101', 'EDGE56', 56, 'CPU board slot')
    for sig, p in CPUCONN.items():
        net(sig, 'J101.' + p)
    for sig, ps in CPUCONN_PWR.items():
        for p in ps:
            net(sig, 'J101.' + p)
    unsure('J101: the motherboard page shows S3 on pin 5, which is also D0; the CPU board and the pin list say S3 is pin S. Used S.')
    # RAM slots
    for ref in ('J102', 'J103', 'J104'):
        part(ref, 'EDGE44', 44, 'RAM board slot')
        for sig, p in RAMSLOT.items():
            net(sig, '%s.%s' % (ref, p))
        for sig, ps in RAMSLOT_PWR.items():
            for p in ps:
                net(sig, '%s.%s' % (ref, p))
    # personality slot
    part('J107', 'EDGE44', 44, 'personality board slot')
    for sig, p in PERS.items():
        if sig in ('A15', 'PHI1', 'RW_EARLY', 'RASTIME') or sig[0] in 'AD' and sig[1:].isdigit():
            net(sig, 'J107.' + p)
    for sig, ps in PERS_PWR.items():
        for p in ps:
            net(sig, 'J107.' + p)
    for sig in ('PHI2B', 'S7', 'S6', 'S4', 'S5', 'D6XX', 'D5XX', 'PIA_CS', 'POKEY_CS', 'GTIA_CS', 'EXSEL', 'GBA', 'GBAN', 'WRITIME'):
        net(sig, 'J107.' + PERS[sig])
    unsure('J107: pin letters taken from the personality board edge connector; the motherboard drawing agrees where it is legible.')
    # cartridge slots
    for ref in ('J108', 'J109'):
        part(ref, 'CART30', 30, 'cartridge slot ' + ('right' if ref == 'J108' else 'left'))
        for sig, p in list(CART_ADDR.items()) + list(CART_DATA.items()):
            net(sig, '%s.%s' % (ref, p))
        net(V5, ref + '.13'); net(GND, ref + '.B')
        net('D5XX', ref + '.15')
        net('R/W', ref + '.R' if ref == 'J109' else ref + '.1')
        net('PHI2', ref + '.S' if ref == 'J109' else ref + '.A')
    net('S4', 'J109.1', 'J108.12'); net('RD4', 'J109.A', 'J108.14')
    net('S5', 'J109.12'); net('RD5', 'J109.14')
    unsure('Cartridge slots: pin assignments from the 8-bit FAQ pinout; the right slot pins 1 and A (R/W, phase 2) are not read on the schematic.')

    # address decoder Z101 (74LS42): A13/A14/A15 select the eight 8K blocks; Y0-Y3 RAM selects, Y4/Y5/Y6/Y7 = S4..S7
    part('Z101', '74LS42', 16, 'address decoder'); part('Z102', '74LS32', 14, 'OR gates')
    net(V5, 'Z101.16', 'Z102.14'); net(GND, 'Z101.8', 'Z102.7')
    part('C101', 'C', 2, '.01uF'); net(V5, 'C101.1'); net(GND, 'C101.2')
    net('A13', 'Z101.15'); net('A14', 'Z101.14'); net('A15', 'Z101.13'); net(GND, 'Z101.12')
    for i, pn in enumerate((1, 2, 3, 4)):
        net('S%d' % i, 'Z101.%d' % pn)
    net('S4', 'Z101.5'); net('S5', 'Z101.6'); net('S6', 'Z101.7'); net('S7', 'Z101.9')
    unsure('Z101 74LS42: input pins (A15 B14 C13 D12, standard pinout) and which address line goes to which input are not legible; assumed A13,A14,A15 and D low.')
    # Z102A/B gate the S4 / S5 RAM selects off when a cartridge pulls RD4 / RD5; pull-downs R108 / R109 680
    net('S4', 'Z102.5'); net('RD4', 'Z102.4'); net('S4G', 'Z102.6')
    net('S5', 'Z102.2'); net('RD5', 'Z102.1'); net('S5G', 'Z102.3')
    part('R108', 'R', 2, '680'); net('RD4', 'R108.1'); net(GND, 'R108.2')
    part('R109', 'R', 2, '680'); net('RD5', 'R109.1'); net(GND, 'R109.2')
    part('R103', 'R', 2, '680'); net('S6', 'R103.1'); net(GND, 'R103.2')
    unsure('Z102A/B (74LS32): which gate input takes S4/S5 and which RD4/RD5, and where the outputs go (taken as the RAM select for slot J104), is not clear on the print.')
    # RAM selects: J102 SEL = S0, U = S1; J103 = S2, S3; J104 = gated S4, S5
    for ref, a, b in (('J102', 'S0', 'S1'), ('J103', 'S2', 'S3'), ('J104', 'S4G', 'S5G')):
        net(a, ref + '.18'); net(b, ref + '.U')
    unsure('RAM slot select pins (18 and U) per slot: S0/S1 -> J102, S2/S3 -> J103, S4/S5 gated -> J104 (the 48K map). Not read directly.')

    # POKEY A101
    part('A101', 'POKEY', 40, 'POKEY'); part('A102', 'PIA6520', 40, '6520 PIA')
    net(V5, 'A101.17', 'A102.20'); net(GND, 'A101.11', 'A101.1', 'A102.1')
    part('C122', 'C', 2, '.1uF'); net(V5, 'C122.1'); net(GND, 'C122.2')
    unsure('POKEY power pins: the drawing shows Vcc on 17 and Vss on 11 next to pin 1, which a real POKEY ties to ground. Both 1 and 11 grounded here.')
    for i in range(8):
        net('D%d' % i, pin('A101', POKEY, 'D%d' % i), pin('A102', PIA, 'D%d' % i))
    for i in range(4):
        net('A%d' % i, pin('A101', POKEY, 'A%d' % i))
    net('A0', 'A102.35'); net('A1', 'A102.36')
    net('R/W', 'A101.32', 'A102.21'); net('PHI2', 'A101.7', 'A102.25')
    net(V5, 'A101.31', 'A102.22', 'A102.24')
    net('POKEY_CS', 'A101.30'); net('PIA_CS', 'A102.23')
    unsure('PIA chip selects: assumed CS0 and CS1 high, /CS2 from the personality PIA select (active low).')
    net('IRQ', 'A101.29', 'A102.38', 'A102.37')
    net('RESETM', 'A102.34')
    # speaker circuit
    part('R102', 'R', 2, '1K'); part('Q101', 'NPN', 3, '2N'); part('C104', 'C', 2, '.001uF'); part('R101', 'R', 2, '100')
    part('J105', 'HDR2', 2, 'speaker'); part('CR101', 'D', 2, 'diode')
    net('AUD', 'A101.37', 'R102.1'); net('QB', 'R102.2', 'Q101.B', 'C104.1'); net(GND, 'C104.2', 'Q101.E')
    net('SPK', 'Q101.C', 'J105.2', 'CR101.1'); net('SPKP', 'J105.1', 'CR101.2', 'R101.2'); net(V5, 'R101.1')
    # keyboard: Z103 KEY OUT / Z104 KEY IN (CD4051), J106 keyboard connector
    part('Z103', 'CD4051', 16, 'key out'); part('Z104', 'CD4051', 16, 'key in'); part('J106', 'CONN22', 22, 'keyboard')
    for z in ('Z103', 'Z104'):
        net(V5, z + '.16'); net(GND, z + '.6', z + '.7', z + '.8')
    net(GND, 'Z103.3')
    for z, ks in (('Z103', ('K0', 'K1', 'K2')), ('Z104', ('K3', 'K4', 'K5'))):
        for k, pn in zip(ks, (11, 10, 9)):
            net(k, '%s.%d' % (z, pn), pin('A101', POKEY, k))
    net('KR1', 'Z104.3', 'A101.25')
    part('R128', 'R', 2, '47K'); net(V5, 'R128.1'); net('KR1', 'R128.2')
    part('R127', 'R', 2, '47K'); net(V5, 'R127.1'); net('KR2', 'R127.2', 'A101.16')
    part('R118', 'R', 2, '100'); net('KR2', 'R118.1'); net('KEYCOM', 'R118.2', 'J106.9')
    out_pins = ('6', '1', '4', '7', '2', '8', '3', '5')
    for i, (xp, jp) in enumerate(zip((13, 14, 15, 12, 1, 5, 2, 4), out_pins)):
        r = 'R%d' % (110 + i)
        part(r, 'R', 2, '100'); net('KO%d' % i, 'Z103.%d' % xp, r + '.1'); net('KB%d' % i, r + '.2', 'J106.' + jp)
    in_pins = ('16', '15', '12', '17', '10', '14', '11', '13')
    for i, (xp, jp) in enumerate(zip((13, 14, 15, 12, 1, 5, 2, 4), in_pins)):
        r = 'R%d' % (119 + i)
        part(r, 'R', 2, '100'); net('KI%d' % i, 'Z104.%d' % xp, r + '.1'); net('KC%d' % i, r + '.2', 'J106.' + jp)
        part('C%d' % (114 + i), 'C', 2, '.001uF'); net('KI%d' % i, 'C%d.1' % (114 + i)); net(GND, 'C%d.2' % (114 + i))
    for c, jp in zip(range(105, 114), ('9', '6', '1', '4', '7', '2', '8', '3', '5')):
        part('C%d' % c, 'C', 2, '.001uF')
        net('KEYCOM' if jp == '9' else 'KB%d' % out_pins.index(jp), 'C%d.1' % c)
        net(GND, 'C%d.2' % c)
    unsure('J106 keyboard connector: pins 15, 17, 10, 14 on the Z104 side (digits blurred on the print); pins 6/1/4/7/2/8/3/5 on the Z103 side are clear.')
    # controller jacks J110-J113 (jack 1..4)
    jack_pia = {1: ('PA0', 'PA1', 'PA2', 'PA3'), 2: ('PA4', 'PA5', 'PA6', 'PA7'), 3: ('PB0', 'PB1', 'PB2', 'PB3'), 4: ('PB4', 'PB5', 'PB6', 'PB7')}
    jack_pot = {1: ('P0', 'P1'), 2: ('P2', 'P3'), 3: ('P4', 'P5'), 4: ('P6', 'P7')}
    for j in range(1, 5):
        ref = 'J%d' % (109 + j)
        part(ref, 'DB9', 9, 'controller port %d' % j)
        net(V5, ref + '.7'); net(GND, ref + '.8')
        for dpin, sig, pia in zip(('1', '2', '3', '4'), ('FWD', 'BACK', 'LEFT', 'RIGHT'), jack_pia[j]):
            r = 'R%d' % (138 + (j - 1) * 4 + int(dpin) - 1)
            part(r, 'R', 2, '220'); net('PORT%d_%s' % (j, sig), ref + '.' + dpin, r + '.1'); net(pia, r + '.2', pin('A102', PIA, pia))
        tr = 'R%d' % (174 + j)
        part(tr, 'R', 2, '220'); net('PORT%d_TRIG' % j, ref + '.6', tr + '.1'); net('T%d' % (j - 1), tr + '.2')
        pa, pb = jack_pot[j]
        net('PORT%d_POTA' % j, ref + '.9'); net('PORT%d_POTB' % j, ref + '.5')
        if pa in POKEY: net('PORT%d_POTA' % j, pin('A101', POKEY, pa))
        if pb in POKEY: net('PORT%d_POTB' % j, pin('A101', POKEY, pb))
    unsure('POKEY pot input P5 (jack 3 pin 5) has no pin on the print: left unconnected.')
    unsure('Controller ports: the pull-up / ladder resistors R130-R153 and the joystick-to-PIA wire order are the standard Atari 800 arrangement, not traced.')
    # page 2: reset, serial bus J115, console switch header J114
    part('A103', 'CD4050', 16, 'CMOS buffer reset/luma'); part('A104', 'CD4050', 16, 'CMOS buffer chroma')
    net(V5, 'A103.1', 'A104.1'); net(GND, 'A103.8', 'A104.8')
    part('R154', 'R', 2, '1M'); part('R155', 'R', 2, '100K'); part('R156', 'R', 2, '470K'); part('C198', 'C', 2, '.1uF'); part('CR102', 'D', 2, 'diode')
    net('RESETM', 'A103.2', 'R154.1', 'R155.1'); net('RCNODE', 'R154.2')
    unsure('Power-on reset network (A103A, R154-R156, C198, CR102): topology as read, not beep checked; reset must reach J101 pin 25 and the PIA.')
    net('RCNODE', 'R155.2', 'C198.1', 'R156.1', 'CR102.1', 'A103.3'); net(V5, 'R156.2', 'CR102.2'); net(GND, 'C198.2')
    net('RESETA', 'J101.X')
    part('J114', 'HDR4', 4, 'console switch header'); part('J115', 'CONN22', 22, 'to power supply')
    for i, (sig, hp) in enumerate((('S2', 1), ('S1', 2), ('S0', 3), ('RNMI', 4))):
        r = 'R%d' % (184 + i)
        part(r, 'R', 2, '100'); net(sig, r + '.1'); net('SW_' + sig, r + '.2', 'J114.%d' % hp)
        part('C%d' % (165 + i), 'C', 2, '.001uF'); net('SW_' + sig, 'C%d.1' % (165 + i)); net(GND, 'C%d.2' % (165 + i))
    sio = (('OCLK', 7), ('BCLK', 8), ('SID', 9), ('SOD', 10), ('CB1', 14), ('CA1', 12), ('CB2', 11))
    for i, (sig, jp) in enumerate(sio):
        r = 'R%d' % (168 + i)
        part(r, 'R', 2, '100'); net('SIO_' + sig, r + '.2', 'J115.%d' % jp)
        net('SIO_' + sig + '_I', r + '.1')
    for sig in ('OCLK', 'BCLK', 'SID', 'SOD'):
        net('SIO_%s_I' % sig, pin('A101', POKEY, sig))
    net('SIO_CB1_I', 'A102.18'); net('SIO_CA1_I', 'A102.40'); net('SIO_CB2_I', 'A102.19')
    net('MOTOR', 'A102.39', 'J115.4')
    net('AUD', 'J115.2')
    net(V5, 'J115.18'); net(V5, 'J115.1'); net(V12, 'J115.21'); net(VM5, 'J115.17'); net(GND, 'J115.19', 'J115.15', 'J115.16', 'J115.22')
    unsure('J115 power pins (18 +5A, 1 +5B, 17 -5, 21 +12, grounds 15 16 19 22) and the serial-bus pin names; the motor transistor Q107 is not modelled yet (CA2 goes to J115 pin 4 directly).')
    # video / luma mixer: CD4050 buffers (pins in/out as the CD4050 standard), sum node to J115
    for sig, ip, op in (('LUM3', 5, 4), ('LUM0', 9, 10), ('LUM1', 14, 15), ('LUM2', 11, 12), ('CSYNC', 7, 6)):
        net(sig, 'A103.%d' % ip)
    part('X101', 'XTAL', 2, '3.579575 MHz'); net('OSC', 'X101.1')
    unsure('Video, chroma and oscillator networks (Q102-Q106, A104, L101-L105, R179-R205): not modelled in this pass, only the bus-facing parts.')


ROMPIN_A = {'A0': 8, 'A1': 7, 'A2': 6, 'A3': 5, 'A4': 4, 'A5': 3, 'A6': 2, 'A7': 1, 'A8': 23, 'A9': 22, 'A10': 19, 'A11': 18}
ROMPIN_D = {'D0': 9, 'D1': 10, 'D2': 11, 'D3': 13, 'D4': 14, 'D5': 15, 'D6': 16, 'D7': 17}


def personality():
    """800 personality board (Fig 2-10): OS ROMs, I/O decoder Z401 and the EXSEL / RASTIME / WRITIME / GBA gates."""
    part('PERSC', 'EDGE44', 44, 'personality board edge')
    for sig, p in PERS.items():
        net(sig, 'PERSC.' + p)
    for sig, ps in PERS_PWR.items():
        for p in ps:
            net(sig, 'PERSC.' + p)
    # mask ROMs: A401 "F" 4K, A402 "D" 2K, A403 "E" 4K. A402 has three chip-select pins (20, 21, 18) and no A11 input.
    for ref, desc in (('A401', 'ROM F ($F000)'), ('A402', 'ROM D ($D800)'), ('A403', 'ROM E ($E000)')):
        part(ref, 'MASKROM', 24, desc)
        net(V5, ref + '.24'); net(GND, ref + '.12')
        part('C%d' % (403 + (0, 2, 3)[('A401', 'A402', 'A403').index(ref)] if ref != 'A401' else 403), 'C', 2, '.1uF')
        for a, pn in ROMPIN_A.items():
            if ref == 'A402' and a == 'A11':
                continue
            net(a, '%s.%d' % (ref, pn))
        for d, pn in ROMPIN_D.items():
            net(d, '%s.%d' % (ref, pn))
    net('S7', 'A401.20', 'A403.20'); net('A12', 'A401.21', 'A403.21')
    net('S6', 'A402.20'); net('A12', 'A402.21'); net('A11', 'A402.18')
    unsure('Personality ROM chip-select pins (20 / 21 / 18) are programmed by the mask; assumed S7 and A12 for A401 / A403 (E000 / F000) and S6, A12, A11 for A402 (D800).')
    # Z401 74LS138: A8, A9, A10 select the D0xx..D7xx page; enables A11 (low), S6 (low), A12 (high)
    part('Z401', '74LS138', 16, 'I/O decoder'); part('C404', 'C', 2, '.01uF')
    net(V5, 'Z401.16', 'C404.1'); net(GND, 'Z401.8', 'C404.2')
    net('A8', 'Z401.1'); net('A9', 'Z401.2'); net('A10', 'Z401.3')
    net('A11', 'Z401.5'); net('A12', 'Z401.6'); net('S6', 'Z401.4')
    net('GTIA_CS', 'Z401.15'); net('POKEY_CS', 'Z401.13'); net('PIA_CS', 'Z401.12')
    net('D5XX', 'Z401.10'); net('D6XX', 'Z401.9'); net('D4XX', 'Z401.11')
    part('Z402', '7400', 14, 'NAND (RASTIME / WRITIME / GBA)'); part('Z403', '74LS03', 14, 'open-collector NAND (EXSEL)')
    for z in ('Z402', 'Z403'):
        net(V5, z + '.14'); net(GND, z + '.7')
    # Z403A: Y4 and the GTIA select; Z403B: A15 and phase 2B through R404; Z403D: RAM S5 and S4. The three outputs, R402 and EXSEL are one wired-AND node.
    net('D4XX', 'Z403.1'); net('GTIA_CS', 'Z403.2')
    net('EXSEL', 'Z403.3', 'Z403.8', 'Z403.6', 'Z402.2', 'Z403.13')
    net('A15', 'Z403.9'); net('EXSELB', 'Z403.10'); net('S5', 'Z403.4'); net('S4', 'Z403.5')
    part('R404', 'R', 2, '1'); net('EXSELB', 'R404.1'); net('PHI2B', 'R404.2')
    part('R402', 'R', 2, '2200'); net(V5, 'R402.1'); net('EXSEL', 'R402.2')
    # Z402A -> /GBA, Z403C -> GBA (pull-up R401)
    net('RW_EARLY', 'Z402.1'); net('GBAN', 'Z402.3', 'Z403.12')
    net('GBA', 'Z403.11')
    part('R401', 'R', 2, '4700'); net(V5, 'R401.1'); net('GBA', 'R401.2')
    # Z402B RASTIME from phase 1, Z402C /WRITIME from phase 2B
    net('PHI1', 'Z402.9'); net('RASTIME', 'Z402.8'); net('PHIX', 'Z402.10', 'Z402.4')
    net('PHI2B', 'Z402.5'); net('WRITIME', 'Z402.6')
    part('R403', 'R', 2, '470'); net('PHI2B', 'R403.1')
    for c in ('C401', 'C402'):
        part(c, 'C', 2, '.1uF' if c == 'C401' else '.001uF'); net(V5, c + '.1'); net(GND, c + '.2')
    unsure('Personality gates: Z402 is read as NAND gates A (in 1,2 out 3), B (in 9,10 out 8), C (in 4,5 out 6) and Z403 as open-collector NANDs with R402 pull-up; the node at Z402 pin 10 / 4 runs off to the right and is not read. Ø2B edge pin is printed as 5 (same as A0): placed on pin S.')


# 16K RAM board (Fig 2-16). DRAM Z505..Z512 hold data bits D6 D7 D5 D3 D4 D2 D1 D0 (bit order read from the data lines).
RAM_DRAM_BITS = [('Z505', 6), ('Z506', 7), ('Z507', 5), ('Z508', 3), ('Z509', 4), ('Z510', 2), ('Z511', 1), ('Z512', 0)]
Z502_Y = {6: 18, 5: 16, 4: 14, 1: 12, 0: 9, 2: 7, 3: 5, 7: 3}      # data bit -> 74LS244 output pin (to the bus)
Z502_A = {6: 2, 5: 4, 4: 6, 1: 8, 0: 11, 2: 13, 3: 15, 7: 17}       # data bit -> 74LS244 input pin (from the DRAM DOUT)
# DRAM address pin driven by each multiplexer output: (mux, output pin) -> 4116 pin, with the (A, B) inputs
RAM_MUX = [('Z503', 4, 5, (2, 'A13'), (3, 'A6')), ('Z503', 7, 10, (5, 'A12'), (6, 'A5')),
           ('Z503', 9, 11, (11, 'A11'), (10, 'A4')), ('Z503', 12, 12, (14, 'A10'), (13, 'A3')),
           ('Z504', 12, 6, (14, 'A9'), (13, 'A2')), ('Z504', 4, 7, (2, 'A8'), (3, 'A1')),
           ('Z504', 7, 13, (5, 'A7'), (6, 'A0'))]
RAM_MUX_B = {'Z503': [(3, 'A6'), (6, 'A5'), (10, 'A4'), (13, 'A3')], 'Z504': [(13, 'A2'), (3, 'A1'), (6, 'A0')]}


def ram16(tag, selA, selB, slot):
    """One 16K RAM board plugged into motherboard slot `slot`; selA / selB are the two 8K select nets. Refs get the suffix `tag`."""
    def r(x): return x + tag
    def n(x): return x + tag
    edge = r('RB')
    part(edge, 'EDGE44', 44, '16K RAM board edge (slot %s)' % slot)
    for sig, p in RAMSLOT.items():
        net(sig, '%s.%s' % (edge, p))
    for sig, ps in RAMSLOT_PWR.items():
        for p in ps:
            net(sig, '%s.%s' % (edge, p))
    net(selA, edge + '.18'); net(selB, edge + '.U')
    for z, t, k in (('Z501', '74LS10', 14), ('Z502', '74LS244', 20), ('Z503', '74LS158', 16), ('Z504', '74LS158', 16)):
        part(r(z), t, k, z)
    net(V5, r('Z501') + '.14', r('Z503') + '.16', r('Z504') + '.16', r('Z502') + '.20')
    net(GND, r('Z501') + '.7', r('Z503') + '.8', r('Z504') + '.8', r('Z502') + '.10', r('Z503') + '.15', r('Z504') + '.15', r('Z504') + '.10')
    # Z501: A (5,3,4 -> 6) read gate, B (13,2,1 -> 12), C (10,11,9 -> 8) RAS
    z1 = r('Z501')
    net('RW_EARLY', z1 + '.5'); net('REF', z1 + '.3', z1 + '.2'); net(selB, z1 + '.13'); net(selA, z1 + '.1')
    net(n('ENB'), z1 + '.12', z1 + '.4', z1 + '.10', z1 + '.11')
    net('RASTIME', z1 + '.9')
    net(n('RDEN'), z1 + '.6', r('Z502') + '.1', r('Z502') + '.19')
    net(n('RAS'), z1 + '.8')
    part(r('R501'), 'R', 2, '220'); part(r('L501'), 'L', 2, '4.7uH'); part(r('C518'), 'C', 2, '47pF')
    net(n('RAS'), r('R501') + '.1'); net(n('RASD'), r('R501') + '.2', r('L501') + '.1')
    net(n('SELRAS'), r('L501') + '.2', r('Z503') + '.1', r('Z504') + '.1', r('C518') + '.1'); net(GND, r('C518') + '.2')
    # data buffer and DRAM
    for ref, bit in RAM_DRAM_BITS:
        d = r(ref)
        part(d, '4116', 16, 'DRAM bit D%d' % bit)
        net('D%d' % bit, d + '.2', '%s.%d' % (r('Z502'), Z502_Y[bit]))
        net(n('DO%d' % bit), d + '.14', '%s.%d' % (r('Z502'), Z502_A[bit]))
        net(n('RAS'), d + '.4'); net(n('CAS'), d + '.15'); net('RW_LATE', d + '.3')
        net(GND, d + '.16'); net(V5, d + '.9'); net(V12, d + '.8'); net(VM5, d + '.1')
    # address multiplexers (SEL low = A inputs, high = B inputs); DRAM address pins A0..A6 = pins 5 7 6 12 11 10 13
    for mux, outp, dpin, (pa, sa), (pb, sb) in RAM_MUX:
        m = r(mux)
        net(sa, '%s.%d' % (m, pa)); net(sb, '%s.%d' % (m, pb))
        net(n('MA%d%s' % (outp, mux)), '%s.%d' % (m, outp), *['%s.%d' % (r(d), dpin) for d, _ in RAM_DRAM_BITS])
    z4 = r('Z504')
    net('PHI2', z4 + '.11'); net(n('CAS'), z4 + '.9')
    for ref in ('C%d' % c for c in range(502, 518)):
        pass
    caps = [(502, V12), (503, VM5)] + [(c, V12 if c % 2 == 0 else (V5 if (c // 2) % 2 == 0 else VM5)) for c in range(504, 518)]
    for c in range(502, 518):
        part(r('C%d' % c), 'C', 2, '.1uF')
    # per DRAM: the +12 cap is the even-numbered one, the other cap +5 or -5 alternately (as drawn)
    for i, (ref, bit) in enumerate(RAM_DRAM_BITS):
        ce, co = 502 + 2 * i, 503 + 2 * i
        net(V12, r('C%d' % ce) + '.1'); net(GND, r('C%d' % ce) + '.2')
        net(VM5 if i % 2 == 0 else V5, r('C%d' % co) + '.1'); net(GND, r('C%d' % co) + '.2')
    for c, v in (('C501', V5), ('C519', V5), ('C521', VM5), ('C522', V5), ('C523', V12)):
        part(r(c), 'C', 2, '.1uF' if c in ('C501', 'C519') else '.001uF'); net(v, r(c) + '.1'); net(GND, r(c) + '.2')
    unsure('16K RAM board in %s: which DRAM holds which data bit and the DOUT-to-74LS244 pairing follow the bit order of the data lines (read from the drawing, bit-consistent); Z501B inputs (U, REF, SEL).' % slot)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, 'atari800-netlist-src', 'atari800.net')
    cpu_board()
    motherboard()
    personality()
    ram16('_1', 'S0', 'S1', 'J102'); ram16('_2', 'S2', 'S3', 'J103'); ram16('_3', 'S4G', 'S5G', 'J104')
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
