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
    'S0': '17', 'T3': 'L', 'T2': '10', 'T1': 'K', 'T0': '9', 'GTIA_CS': '27', 'RNMI': 'Z', 'LP': 'A', 'REF': 'D', 'RESETA': 'X',
    'NMI': '23', 'RESETM': '25', 'D7': '8', 'D6': 'J', 'D5': 'H', 'D4': 'F', 'D3': '7', 'D2': '6', 'D1': 'E', 'D0': '5',
    'SYNC': 'BB', 'IRQ': 'AA',
}
CPUCONN_PWR = {'+5V': ['26', 'DD'], '+12V': ['EE'], 'GND': ['28', 'FF']}

# chip pin tables -----------------------------------------------------------------------------------------------
GTIA = {'VCC': 27, 'VSS': 3, 'R/W': 33, 'HALT': 26, 'PHI2': 30, 'A4': 38, 'A3': 39, 'A2': 40, 'A1': 1, 'A0': 2, 'AN0': 18,
        'AN1': 19, 'AN2': 20, 'F0': 29, 'D7': 34, 'D6': 35, 'D5': 36, 'D4': 37, 'D3': 4, 'D2': 5, 'D1': 6, 'D0': 7, 'COL': 21,
        'LUM0': 22, 'LUM1': 23, 'LUM2': 24, 'LUM3': 31, 'OSC': 28, 'CSYNC': 25, 'S3': 15, 'S2': 14, 'S1': 13, 'S0': 12,
        'T3': 11, 'T2': 10, 'T1': 9, 'T0': 8, 'GTIA_CS': 32, 'DEL': 17}
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
        net('Q1N', ref + '.1', ref + '.19')
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
    net('R/W', pin('A301', GTIA, 'R/W'), pin('A302', ANTIC, 'R/W'), 'Z301.13')       # bus R/W: Z301A output (CPU R/W forced to read when /Q1 is high)
    net('CPURW', pin('A303', CPU6502, 'R/W'), 'Z301.8')
    net('HALT', pin('A301', GTIA, 'HALT'), pin('A302', ANTIC, 'HALT'), 'Z302.2')
    net('PHI2', pin('A301', GTIA, 'PHI2'), pin('A302', ANTIC, 'PHI2'), 'Z302.9')
    net('RDY', pin('A302', ANTIC, 'RDY'), pin('A303', CPU6502, 'RDY'))
    net('RNMI', pin('A302', ANTIC, 'RNMI'))
    net('LP', pin('A302', ANTIC, 'LP'))
    net('REF', pin('A302', ANTIC, 'REF'))
    net('NMI', pin('A302', ANTIC, 'NMI'), pin('A303', CPU6502, 'NMI'))
    net('RESETM', pin('A302', ANTIC, 'RESET'), pin('A303', CPU6502, 'RES'))
    net('IRQ', pin('A303', CPU6502, 'IRQ'))
    net('SYNC', pin('A303', CPU6502, 'SYNC'))
    net('PHI0', pin('A303', CPU6502, 'PHI0'))   # Z301 pin 1 (below); ANTIC pin 34 is NOT on this wire (owner board)
    # GTIA video and console
    for s in ('COL', 'LUM0', 'LUM1', 'LUM2', 'LUM3', 'OSC', 'CSYNC', 'S3', 'S2', 'S1', 'S0', 'T3', 'T2', 'T1', 'T0', 'GTIA_CS'):
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
    part('R311', 'R', 2, '4.7K')
    part('R312', 'R', 2, '1'); part('R313', 'R', 2, '1')
    # clock generator Z301 / Z302 (read from the 300 dpi drawing 2-12): Z302 flop 2 makes PHI2 (Q2) and PHI1 (/Q2), flop 1 (D1 = HALT, clock = /Q2)
    # makes /Q1 which enables the address buffers and forces the bus R/W high through Z301B / Z301A; Z301C makes the 6502 PHI0.
    net('PHI1', 'Z302.8', 'Z302.3')
    net('Q1N', 'Z302.6', 'Z301.9', 'Z301.2')          # /Q1: buffer enables, Z301B input, Z301C input
    net('Z301B_OUT', 'Z301.10', 'Z301.11', 'Z301.12')  # Z301B output into Z301A (an inverter, inputs tied)
    net('Z301D_OUT', 'Z301.4', 'Z301.3', 'Z302.10')   # Z301D output: Z301C input and Z302 PRST2
    net('F0H', pin('A302', ANTIC, 'PHI0'), 'Z301.6', 'Z302.13')   # scope on the owner's board: 1.79 MHz here. R310 is NOT on this wire (both its ends read flat high).
    net(GND, 'Z301.5')
    net('PHI0', 'Z301.1')                              # Z301C output = the 6502 and ANTIC phase-0 input
    net(V5, 'R311.1')
    net('RDY', 'R311.2')
    unsure('CPU board clock: on the owner board Z302 pin 13 and Z301 pin 6 carry the 1.79 MHz F0 clock (scope), but there is no continuity from them to GTIA 29 or ANTIC 35, which beep to each other. How the clock gets onto that wire is not known. R310 is not on it: both ends read flat high, and its far end does not beep to GTIA 29. Where R310 really goes is not known.')
    unsure('CPU board: 6502 RDY pin printed as 7 next to SYNC 7; assumed pin 2 (RDY) joined to ANTIC RDY.')


# ---------------------------------------------------------------------------------------------------------------
# Motherboard CO14175 (Fig 2-6A page 1, 2-6B page 2). J101 is the CPU slot, J102-J104 the RAM slots, J107 the personality slot,
# J108 (right) and J109 (left) the cartridge slots. All the bus nets are shared by name with the plug-in boards (flattened).
RAMSLOT = {'A0': '5', 'A1': '7', 'A2': 'E', 'A3': '10', 'A4': 'J', 'A5': '9', 'A6': '12', 'A7': '6', 'A8': '8', 'A9': 'F',
           'A10': '11', 'A11': 'K', 'A12': 'L', 'A13': 'H', 'D0': '1', 'D1': 'A', 'D2': '2', 'D3': '3', 'D4': 'B', 'D5': 'C',
           'D6': 'D', 'D7': '4', 'PHI2': '14', 'RASTIME': '15', 'R/W': '16', 'RW_LATE': '13', 'REF': '17'}
RAMSLOT_PWR = {'+5V': ['19', 'W'], '-5V': ['20', 'X'], '+12V': ['21', 'Y'], 'GND': ['22', 'Z']}
PERS = {'D0': '1', 'D1': 'A', 'D2': '2', 'D3': '3', 'D4': 'B', 'D5': 'C', 'D6': 'D', 'D7': '4', 'A0': '5', 'A1': '7', 'A2': 'E',
        'A3': '10', 'A4': 'K', 'A5': '9', 'A6': 'J', 'A7': '6', 'A8': '8', 'A9': 'F', 'A10': '11', 'A11': 'L', 'A12': '12',
        'A15': 'R', 'PHI1': '17', 'R/W': 'U', 'RASTIME': 'V', 'PHI2': 'S', 'S7': 'H', 'S6': 'M', 'S4': 'P', 'S5': 'N',
        'D6XX': 'W', 'D5XX': 'X', 'PIA_CS': '18', 'POKEY_CS': '19', 'GTIA_CS': '13', 'EXSEL': '14', 'GBA': 'T', 'GBAN': '15',
        'WRITIME': '16'}
PERS_PWR = {'+5V': ['21', 'Y'], 'GND': ['22', 'Z']}
# cartridge slot pins; address and data lines are the same on both slots
CART_ADDR = {'A0': '5', 'A1': '4', 'A2': '3', 'A3': '2', 'A4': 'C', 'A5': 'D', 'A6': 'E', 'A7': 'F', 'A8': 'H', 'A9': 'J',
             'A10': 'P', 'A11': 'N', 'A12': 'K'}
CART_DATA = {'D0': '10', 'D1': '9', 'D2': '8', 'D3': 'L', 'D4': '6', 'D5': '7', 'D6': '11', 'D7': 'M'}
POKEY = {'VSS': 1, 'D3': 2, 'D4': 3, 'D5': 4, 'D6': 5, 'D7': 6, 'PHI2': 7, 'P5': 11, 'VCC': 17, 'KR2': 16, 'KR1': 25,
         'K5': 18, 'K4': 19, 'K3': 20, 'K2': 21, 'K1': 22, 'K0': 23, 'SID': 24, 'BCLK': 26, 'OCLK': 27, 'SOD': 28, 'IRQ': 29,
         'CS0': 30, 'CS1': 31, 'RW': 32, 'A3': 33, 'A2': 34, 'A1': 35, 'A0': 36, 'AUD': 37, 'D0': 38, 'D1': 39, 'D2': 40,
         'P1': 15, 'P0': 14, 'P3': 13, 'P2': 12, 'P4': 10, 'P7': 9, 'P6': 8}
PIA = {'VSS': 1, 'PA0': 2, 'PA1': 3, 'PA2': 4, 'PA3': 5, 'PA4': 6, 'PA5': 7, 'PA6': 8, 'PA7': 9, 'PB0': 10, 'PB1': 11,
       'PB2': 12, 'PB3': 13, 'PB4': 14, 'PB5': 15, 'PB6': 16, 'PB7': 17, 'CB1': 18, 'CB2': 19, 'VCC': 20, 'RW': 21, 'CS0': 22,
       'CS2': 23, 'CS1': 24, 'ENABLE': 25, 'D7': 26, 'D6': 27, 'D5': 28, 'D4': 29, 'D3': 30, 'D2': 31, 'D1': 32, 'D0': 33,
       'RESET': 34, 'RS1': 35, 'RS0': 36, 'IRQB': 37, 'IRQA': 38, 'CA2': 39, 'CA1': 40}
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
        if sig in ('A15', 'PHI1', 'R/W', 'RASTIME') or sig[0] in 'AD' and sig[1:].isdigit():
            net(sig, 'J107.' + p)
    for sig, ps in PERS_PWR.items():
        for p in ps:
            net(sig, 'J107.' + p)
    for sig in ('PHI2', 'S7', 'S6', 'S4', 'S5', 'D6XX', 'D5XX', 'PIA_CS', 'POKEY_CS', 'GTIA_CS', 'EXSEL', 'GBA', 'GBAN', 'WRITIME'):
        net(sig, 'J107.' + PERS[sig])
    unsure('J107: pin letters taken from the personality board edge connector; the motherboard drawing agrees where it is legible.')
    # cartridge slots
    for ref in ('J108', 'J109'):
        part(ref, 'CART30', 30, 'cartridge slot ' + ('right' if ref == 'J108' else 'left'))
        for sig, p in list(CART_ADDR.items()) + list(CART_DATA.items()):
            net(sig, '%s.%s' % (ref, p))
        net(V5, ref + '.13'); net(GND, ref + '.B')
        net('D5XX', ref + '.15')
        if ref == 'J108':
            net('R/W', 'J108.1'); net('PHI2', 'J108.A')   # owner board: PHI2 on the J108 pin opposite 1 (check 14); R/W on pin 1 is the first reading of the print, not measured
        else:
            net('R/W', ref + '.R'); net('PHI2', ref + '.S')
    net('S4', 'J109.1', 'J108.12'); net('RD4', 'J109.A', 'J108.14')
    net('S5', 'J109.12'); net('RD5', 'J109.14')
    # the cartridge itself: one ROM part wired to the left slot (empty until a cartridge is loaded)
    part('CARTL', 'CARTROM', 24, 'cartridge ROM (left slot)')
    for i in range(13): net('A%d' % i, 'CARTL.%d' % (i + 1))
    for i in range(8): net('D%d' % i, 'CARTL.%d' % (14 + i))
    net('S5', 'CARTL.22'); net('RD5', 'CARTL.23')
    unsure('Cartridge slots: the print labels pin S RAS and pin R R/WE on both slots. Owner board (check 14): J104 pin 14 (PHI2) and ANTIC PHI2 beep to the J108 pin opposite pin 1 (A), so J108 has PHI2 on A and (first reading of the print, not measured) R/W on pin 1; J109 has no PHI2 on any pin. J109 S and R stay as the netlist had them until measured.')

    # address decoder Z101 (74LS42): A13/A14/A15 select the eight 8K blocks; Y0-Y3 RAM selects, Y4/Y5/Y6/Y7 = S4..S7
    part('Z101', '74LS42', 16, 'address decoder'); part('Z102', '74LS32', 14, 'OR gates')
    net(V5, 'Z101.16', 'Z102.14'); net(GND, 'Z101.8', 'Z102.7')
    part('C101', 'C', 2, '.01uF'); net(V5, 'C101.1'); net(GND, 'C101.2')
    net('A13', 'Z101.15'); net('A14', 'Z101.14'); net('A15', 'Z101.13'); net('EXSEL', 'Z101.12')
    for i, pn in enumerate((1, 2, 3, 4)):
        net('MS%d' % i, 'Z101.%d' % pn)
    net('S4', 'Z101.5'); net('S5', 'Z101.6'); net('S6', 'Z101.7'); net('S7', 'Z101.9')
    unsure('Z101 74LS42: read at 330 dpi: A (pin 15) = A13, B (14) = A14, C (13) = A15 and D (12) = EXSEL (not ground); Y6 and Y7 go to J107 M and H. R103 680 hangs from the EXSEL line, not from S6 as first netted.')
    # Z102A/B gate the S4 / S5 RAM selects off when a cartridge pulls RD4 / RD5; pull-downs R108 / R109 680
    net('S4', 'Z102.5'); net('RD4', 'Z102.4'); net('S4G', 'Z102.6')
    net('S5', 'Z102.2'); net('RD5', 'Z102.1'); net('S5G', 'Z102.3')
    part('R108', 'R', 2, '680'); net('RD4', 'R108.1'); net(GND, 'R108.2')
    part('R109', 'R', 2, '680'); net('RD5', 'R109.1'); net(GND, 'R109.2')
    part('R103', 'R', 2, '680'); net('EXSEL', 'R103.1'); net(GND, 'R103.2')
    # Z102C/D: R/W LATE for the RAM slots = R/W OR WRITIME (low only in a write, during the write strobe)
    net('R/W', 'Z102.9', 'Z102.12'); net('WRITIME', 'Z102.10', 'Z102.13'); net('RW_LATE', 'Z102.8', 'Z102.11')
    unsure('Z102C/D: R/W LATE taken as R/W OR WRITIME (inputs 9,10 / 12,13, outputs 8 / 11); the W/TIME source is the personality.')
    unsure('Z102A/B (74LS32): read at 330 dpi: A has Y4 (decoder pin 5) on pin 5 and RD4 on pin 4 (R108 680 to ground), output 6 = gated S4 to J107 P and J103 P; B has Y5 on pin 1 and RD5 on pin 2 (R109 680 to ground), output 3 = gated S5 to J107 N and J103 M. The cartridge slots take the raw Y4 / Y5 (S4 on J109 pin 1 and J108 pin 12, S5 on J109 pin 12). The personality slot P and N pins are wired to the raw S4 / S5 here, the print shows the gated ones.')
    # RAM selects (decoder outputs MS0-MS3; the console switch lines of the CPU slot are S0-S3): J104 (slot 1) 18 = MS0, U = MS1; J103 (slot 2) = MS2, MS3; J102 (slot 3) = gated S4, S5
    for ref, a, b in (('J104', 'MS0', 'MS1'), ('J103', 'MS2', 'MS3'), ('J102', 'S4G', 'S5G')):
        net(a, ref + '.18'); net(b, ref + '.U')
    unsure('RAM slot select pins (18 and U) per slot: the print shows Y0/Y1 on J104 pins 18 and U (slot 1, so the first 16K board is in J104, not J102), and Y2/Y3 reaching J103; the gated S4/S5 (Z102A/B outputs) go to J107 P and N and to J103 P and M, and the pins that join J103 18/T/R/N to J104 T/S/P/M and J102 U/18 to J104 N/R are not understood. Taken: J104 = Y0/Y1, J103 = Y2/Y3, J102 = gated S4/S5 on pins 18 and U. See beep check 12.')

    # POKEY A101
    part('A101', 'POKEY', 40, 'POKEY'); part('A102', 'PIA6520', 40, '6520 PIA')
    net(V5, 'A101.17', 'A102.20'); net(GND, 'A101.1', 'A102.1')
    part('C122', 'C', 2, '.1uF'); net(V5, 'C122.1'); net(GND, 'C122.2')
    for i in range(8):
        net('D%d' % i, pin('A101', POKEY, 'D%d' % i), pin('A102', PIA, 'D%d' % i))
    for i in range(4):
        net('A%d' % i, pin('A101', POKEY, 'A%d' % i))
    net('A0', 'A102.36'); net('A1', 'A102.35')
    unsure('PIA register select: the print labels pin 35 RS0 and pin 36 RS1, the 6520 has RS0 on 36 and RS1 on 35 (the Atari OS needs A0 on RS0): wired as the chip, A0 to pin 36, A1 to pin 35.')
    net('R/W', 'A101.32', 'A102.21'); net('PHI2', 'A101.7', 'A102.25')
    net(V5, 'A101.31', 'A102.22', 'A102.24')
    net('POKEY_CS', 'A101.30'); net('PIA_CS', 'A102.23')
    unsure('PIA chip selects: assumed CS0 and CS1 high, /CS2 from the personality PIA select (active low).')
    net('IRQ', 'A101.29', 'A102.38', 'A102.37')
    net('RESETM', 'A102.34')
    # speaker circuit: the print runs the line from J101 (S3, the GTIA speaker bit) through R102 to Q101, not the POKEY audio
    part('R102', 'R', 2, '1K'); part('Q101', 'NPN', 3, '2N'); part('C104', 'C', 2, '.001uF'); part('R101', 'R', 2, '100')
    part('J105', 'HDR2', 2, 'speaker'); part('CR101', 'D', 2, 'diode')
    net('AUD', 'A101.37'); net('S3', 'R102.1'); net('QB', 'R102.2', 'Q101.B', 'C104.1'); net(GND, 'C104.2', 'Q101.E')
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
    unsure('J106 keyboard connector: reread at 330 and 800 dpi, the scan itself is blurred. Z103 side 6/1/4/7/2/8/3/5 and the NC pin 22 are clear; on the Z104 side 16, 12, 17, 14, 11 and 13 are clear and the glyphs at positions 2 and 5 are not, but the pins are 10 and 15 by elimination (the set is 10-17), so only whether position 2 is 15 and position 5 is 10, or the other way round, is open.')
    # controller jacks J110-J113 (jack 1..4)
    jack_pia = {1: ('PA0', 'PA1', 'PA2', 'PA3'), 2: ('PA4', 'PA5', 'PA6', 'PA7'), 3: ('PB0', 'PB1', 'PB2', 'PB3'), 4: ('PB4', 'PB5', 'PB6', 'PB7')}
    POTROW = ('P1', 'P0', 'P3', 'P2', 'P5', 'P4', 'P7', 'P6')  # resistor rows R130-R137 top to bottom
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
        for sg, pp in (('A', pa), ('B', pb)):
            k = POTROW.index(pp)
            part('R%d' % (130 + k), 'R', 2, '1.8K'); part('C%d' % (127 + k), 'C', 2, '.047uF'); part('C%d' % (135 + k), 'C', 2, '.001uF')
            net('POT_' + pp, pin('A101', POKEY, pp), 'R%d.1' % (130 + k), 'C%d.1' % (127 + k))
            net('PORT%d_POT%s' % (j, sg), 'R%d.2' % (130 + k), 'C%d.1' % (135 + k))
            net(GND, 'C%d.2' % (127 + k), 'C%d.2' % (135 + k))
        for d in range(4):
            idx = (j - 1) * 4 + d; cn = 143 + idx if idx < 8 else 153 + idx - 8
            part('C%d' % cn, 'C', 2, '.001uF'); net('PORT%d_%s' % (j, ('FWD', 'BACK', 'LEFT', 'RIGHT')[d]), 'C%d.1' % cn); net(GND, 'C%d.2' % cn)
        part('C%d' % (193 + j), 'C', 2, '.001uF'); net('PORT%d_TRIG' % j, 'C%d.1' % (193 + j)); net(GND, 'C%d.2' % (193 + j))
        if j in (1, 3, 4):
            cv = {1: 123, 3: 151, 4: 152}[j]; part('C%d' % cv, 'C', 2, '.001uF'); net(V5, 'C%d.1' % cv, ref + '.7'); net(GND, 'C%d.2' % cv)
    for i in range(8):
        part('R158' + 'BCDEFGHI'[i], 'R', 2, '4.7K'); net(V5, 'R158%s.1' % 'BCDEFGHI'[i]); net('PB%d' % i, 'R158%s.2' % 'BCDEFGHI'[i])
    unsure('POKEY pins: the print shows P5 on pin 11 and a ground at the foot of the chip labelled 11; a real POKEY has P5 on 11 and Vss on 1, so pin 11 is P5 here and only pin 1 is ground. Which POKEY pot goes to which jack (jack 1 = P0 A / P1 B ... jack 4 = P6 A / P7 B) follows the standard arrangement and the order of the wires out of R130-R137; the wire crossings are not followed one by one.')
    unsure('Controller ports: read at 200 dpi. Jack pins 7 Vcc, 5 B pot, 9 A pot, 1 fwd, 2 back, 3 left, 4 right, 6 trigger, 8 Vss. R130-R137 1.8K in series with the pots (.047uF C127-C134 on the POKEY side, .001 C135-C142 on the jack side), R138-R153 220 on the PIA lines with .001 C143-C150 / C153-C160 on the jack side, R175-R178 220 and C194-C197 on the triggers, R158B-R158I 4.7K pull-ups on PB0-PB7. The pairing of each capacitor with its line is by position, and which wire goes to which jack pin is the standard arrangement, not followed wire by wire.')
    # page 2: reset, serial bus J115, console switch header J114
    part('A103', 'CD4050', 16, 'CMOS buffer reset/luma'); part('A104', 'TRARRAY', 14, 'transistor array (chroma amplifier)')
    net(V5, 'A103.1'); net(GND, 'A103.8')
    part('R154', 'R', 2, '1M'); part('R155', 'R', 2, '100K'); part('R156', 'R', 2, '470K'); part('C198', 'C', 2, '.1uF'); part('CR102', 'D', 2, 'diode')
    net('RESETM', 'A103.2', 'R154.1', 'R155.1'); net('RCNODE', 'R154.2')
    unsure('Power-on reset network (A103A, R154-R156, C198, CR102): topology as read, not beep checked; reset must reach J101 pin 25 and the PIA.')
    net('RCNODE', 'R155.2', 'C198.1', 'R156.1', 'CR102.1', 'A103.3'); net(V5, 'R156.2', 'CR102.2'); net(GND, 'C198.2')
    net('RESETA', 'J101.X')
    part('J114', 'HDR4', 4, 'console switch header'); part('J115', 'CONN22', 22, 'to power supply')
    for i, (sig, hp) in enumerate((('RNMI', 1), ('S0', 4), ('S1', 3), ('S2', 2))):
        r = 'R%d' % (184 + i)
        part(r, 'R', 2, '100'); net(sig, r + '.1'); net('SW_' + sig, r + '.2', 'J114.%d' % hp)
        part('C%d' % (165 + i), 'C', 2, '.001uF'); net('SW_' + sig, 'C%d.1' % (165 + i)); net(GND, 'C%d.2' % (165 + i))
    unsure('J114 pin order: measured on the owner board (beep check 10): ANTIC pin 6 (/RNMI) goes to R184 and J114 pin 1, and J114 pin 2 goes to R187, whose other end is on the J101 pin opposite 14 (S2). J114 pin 4 reaches R185 and pin 3 reaches R186, as in the print (which signal is on which of those two resistors is not measured).')
    sio = (('OCLK', 7), ('BCLK', 8), ('SID', 9), ('SOD', 10), ('CB1', 14), ('CA1', 12), ('CB2', 11))
    for i, (sig, jp) in enumerate(sio):
        r = 'R%d' % (168 + i)
        part(r, 'R', 2, '100'); net('SIO_' + sig, r + '.2', 'J115.%d' % jp)
        net('SIO_' + sig + '_I', r + '.1')
    for sig in ('OCLK', 'BCLK', 'SID', 'SOD'):
        net('SIO_%s_I' % sig, pin('A101', POKEY, sig))
    net('SIO_CB1_I', 'A102.18'); net('SIO_CA1_I', 'A102.40'); net('SIO_CB2_I', 'A102.19')
    # motor control: CA2 -> R159 -> PNP Q107 (emitter +5A) -> L106 -> J115 pin 4, with C189 on the connector side
    part('R159', 'R', 2, '2.7K'); part('Q107', 'PNP', 3, 'PNP'); part('L106', 'L', 2, 'choke'); part('C189', 'C', 2, '.1uF')
    net('MOTOR', 'A102.39', 'R159.1'); net('MOTORB', 'R159.2', 'Q107.B'); net(V5, 'Q107.E')
    net('MOTORC', 'Q107.C', 'L106.1'); net('MOTORJ', 'L106.2', 'J115.4', 'C189.1'); net(GND, 'C189.2')
    # serial bus: 4.7K pull-ups R129A-F on the chip side of R168-R174, R157 1K on AUD, C182-C188 .001 from each connector pin to ground
    for ref, sig in (('R129A', 'SOD'), ('R129B', 'OCLK'), ('R129C', 'BCLK'), ('R129D', 'CA1'), ('R129E', 'SID'), ('R129F', 'CB2'), ('R158A', 'CB1')):
        part(ref, 'R', 2, '4.7K'); net(V5, ref + '.1'); net('SIO_%s_I' % sig, ref + '.2')
    part('R157', 'R', 2, '1K'); net(V5, 'R157.1'); net('AUD', 'R157.2')
    for i, jp in enumerate((7, 8, 9, 10, 14, 12, 11)):
        c = 'C%d' % (182 + i)
        part(c, 'C', 2, '.001uF'); net('SIO_' + 'OCLK BCLK SID SOD CB1 CA1 CB2'.split()[(7, 8, 9, 10, 14, 12, 11).index(jp)], c + '.1'); net(GND, c + '.2')
    unsure('SIO pull-ups: R129 is a 10-pin pack with the common +5 V leg (dotted) as leg 1; measured on the owner board (beep check 11): leg 3 SOD, leg 4 OCLK, leg 5 BCLK, leg 6 CA1, leg 7 SID, leg 10 CB2. Legs 2, 8 and 9 are not identified (CB1 is netted to a separate R158A here). They are called R129A to R129F in the order of the legs 3, 4, 5, 6, 7, 10. C182-C188 top to bottom on pins 7, 8, 9, 10, 14, 12, 11.')
    # supply filters at J115: the capacitors C190-C193, C199 and C200 are netted; the chokes L107-L112 that sit in series on the real board are not,
    # because the simulator feeds the supplies in at the connector pins and a series part would cut the rails
    for ref, v in (('C190', '.1uF'), ('C191', '.1uF'), ('C192', '.1uF'), ('C193', '.1uF'), ('C199', '10uF'), ('C200', '.001uF')): part(ref, 'C', 2, v)
    net(V5, 'J115.18', 'J115.19', 'J115.17', 'C190.1', 'C192.1', 'C200.1'); net(VM5, 'J115.1', 'C191.1'); net(V12, 'J115.21', 'C193.1', 'C199.1')
    net(GND, 'C190.2', 'C191.2', 'C192.2', 'C193.2', 'C199.2', 'C200.2', 'J115.15', 'J115.16', 'J115.22')
    unsure('J115 power pins read at 300 dpi from the column positions: 18 and 19 +5A, 1 -5, 17 +5B, 21 +12, grounds 15 16 22 (earlier I had pins 1 and 17 the other way round). The six chokes L107-L112 (+5A through L111 and L107, -5 L108, +5B L109, +12 L112 and L110) are not netted: they are in series on the real board, and here the rails go straight to the connector because the simulator feeds power in there. C190-C193 are paired with the rails by position. +5A and +5B are one rail.')
    # video / luma mixer: CD4050 buffers (pins in/out as the CD4050 standard), sum node to J115
    for sig, ip, op in (('LUM3', 5, 4), ('LUM0', 9, 10), ('LUM1', 14, 15), ('LUM2', 11, 12), ('CSYNC', 7, 6)):
        net(sig, 'A103.%d' % ip)
    # oscillator: Q102 / Q103 (both PNP, emitters on the +12 side through R104), crystal X101 between Q102's base node and Q103's collector node, OSC taken from Q102's collector (R106 to ground)
    part('X101', 'XTAL', 2, '3.579575 MHz'); part('OSCSIM', 'OSCSRC', 2, 'simulator clock source')
    part('Q102', 'PNP', 3, 'PNP'); part('Q103', 'PNP', 3, 'PNP')
    for ref, v in (('R104', '470'), ('R105', '100'), ('R106', '270'), ('R107', '100')): part(ref, 'R', 2, v)
    part('C103', 'C', 2, '.22uF')
    net(V12, 'R104.1', 'C103.1'); net(GND, 'C103.2', 'R106.2', 'R107.2'); net('OSCE', 'R104.2', 'Q102.E', 'Q103.E')
    net(V5, 'R105.1', 'Q103.B'); net('XTB', 'R105.2', 'Q102.B', 'X101.1'); net('XTC', 'X101.2', 'Q103.C', 'R107.1')
    net('OSC', 'Q102.C', 'R106.1', 'OSCSIM.1')
    unsure('Oscillator (read at 300 dpi): Q102 and Q103 PNP by their emitter arrows, Q103 base to +5A, crystal between the Q102 base / R105 node and the Q103 collector / R107 node, OSC from the Q102 collector with R106 to ground. OSCSIM is not a real part: it stands in for the oscillator and puts the simulator clock on the OSC net, so beep the crystal and transistor pads, not OSCSIM.')
    # video: the luminance mixer (A103B-F through R180-R183 and CR108 into the node at Q105's base), Q105 and the outputs to J115
    part('R179', 'R', 2, '2200'); part('C162', 'C', 2, '.1uF'); part('C163', 'C', 2, '33pF'); part('CR108', 'D', 2, 'diode')
    net(V5, 'R179.1', 'C162.1'); net(GND, 'C162.2'); net('LUMN', 'R179.2', 'C163.1', 'Q105.B', 'CR108.1'); net(GND, 'C163.2')
    for ref, val, op in (('R180', '36K', 4), ('R181', '18K', 10), ('R182', '9.1K', 15), ('R183', '4700', 12)):
        part(ref, 'R', 2, val); net('LUMOUT%d' % op, 'A103.%d' % op, ref + '.1'); net('LUMN', ref + '.2')
    net('SYNCOUT', 'A103.6', 'CR108.2')
    part('Q105', 'NPN', 3, 'NPN'); net(V5, 'Q105.C'); net('EMIT', 'Q105.E')
    part('R189', 'R', 2, '75'); part('R205', 'R', 2, '3900'); part('R200', 'R', 2, '220'); part('R201', 'R', 2, '120')
    part('R202', 'R', 2, '1K'); part('C164', 'C', 2, '100pF'); part('R203', 'R', 2, '270'); part('R204', 'R', 2, '75')
    part('R206', 'R', 2, '18K'); part('C170', 'C', 2, '100pF')
    part('L103', 'L', 2, 'choke'); part('L104', 'L', 2, 'choke'); part('L105', 'L', 2, 'choke')
    part('C174', 'C', 2, '220pF'); part('C175', 'C', 2, '220pF'); part('C176', 'C', 2, '220pF')
    net('EMIT', 'R189.1', 'R205.1', 'R200.1'); net('LUMJ', 'R189.2', 'C174.1', 'J115.5'); net(GND, 'C174.2')
    net('LUMV', 'R200.2', 'R201.2', 'R202.1', 'L104.1'); net(GND, 'R201.1')
    net('VIDJ', 'L104.2', 'C175.1', 'J115.6'); net(GND, 'C175.2')
    net('CH64', 'R202.2', 'C164.1'); net('CHN3', 'C164.2', 'R203.1', 'A104.10')
    net('CHR', 'R203.2', 'R204.1', 'L105.1'); net(GND, 'R204.2'); net('CHRJ', 'L105.2', 'C176.1', 'J115.3'); net(GND, 'C176.2')
    net('MODN', 'R205.2', 'R206.1', 'L103.1'); net('MODJ', 'L103.2', 'J115.20')
    # chroma amplifier: COL -> R196 -> C(10pF) -> A104A base; R197 to ground, R198 feedback and R199 to +5B at its collector, which drives A104B (emitter follower)
    part('R196', 'R', 2, '3.3K'); part('C16X', 'C', 2, '10pF'); part('R197', 'R', 2, '4700'); part('R198', 'R', 2, '10K'); part('R199', 'R', 2, '1K')
    net('COL', 'R196.1'); net('COLR', 'R196.2', 'C16X.1'); net('CBASE', 'C16X.2', 'R197.1', 'R198.1', 'A104.12'); net(GND, 'R197.2', 'A104.13')
    net('CCOLL', 'A104.14', 'R198.2', 'R199.1', 'A104.9', 'C170.2'); net(V5, 'R199.2', 'A104.11')
    net('MODC', 'R206.2', 'C170.1')
    # RF modulator stage: Q106 (NPN) collector on +5B through L101 / L102, base bias R162 from +5B, emitter node RFN with C169 to +5B, R188 to ground, C173 back to the collector;
    # R190 and C171 join RFN to the MOD node
    part('Q106', 'NPN', 3, 'NPN'); part('L101', 'L', 2, 'variable'); part('L102', 'L', 2, '2.0uH')
    for ref, v in (('R162', '1K'), ('R188', '9.1K'), ('R190', '1.8K')): part(ref, 'R', 2, v)
    for ref, v in (('C169', '820pF'), ('C171', '47pF'), ('C173', '820pF')): part(ref, 'C', 2, v)
    net(V5, 'L101.1', 'R162.1', 'C169.1'); net('RFL', 'L101.2', 'L102.1'); net('RFC', 'L102.2', 'Q106.C', 'C173.1'); net('RFB', 'R162.2', 'Q106.B')
    net('RFN', 'Q106.E', 'C169.2', 'R188.1', 'C173.2', 'R190.1', 'C172.2'); net(GND, 'R188.2'); net('RFR', 'R190.2', 'C171.1'); net('MODN', 'C171.2')
    # sound amplifier: A104C / A104D differential pair; C172 and R191 bring RFN to the collector of C, AUD (POKEY) comes in through R167
    for ref, v in (('R163', '2.2K'), ('R164', '3300'), ('R165', '1K'), ('R166', '1K'), ('R167', '18K'), ('R191', '18K'), ('R192', '4.7K'), ('R193', '4.7K'), ('R194', '4700'), ('R195', '4700')):
        part(ref, 'R', 2, v)
    for ref, v in (('C172', '.1uF'), ('C177', '.001uF'), ('C178', '.001uF'), ('C179', '10uF'), ('C180', '.22uF'), ('C181', '.1uF')): part(ref, 'C', 2, v)
    net(V5, 'R163.1', 'R166.1'); net('AMPBC', 'A104.4', 'R163.2', 'R165.1'); net('AMPBD', 'A104.2', 'R165.2', 'C180.2', 'R167.2', 'R195.1')
    net('AMPR', 'R166.2', 'C180.1'); net('AMPE', 'A104.3', 'R164.1'); net(GND, 'R164.2')
    net('AMPC', 'A104.5', 'R191.2', 'R192.1'); net('AMPIN', 'R191.1', 'C172.1'); net('AMPD', 'A104.1', 'R193.1', 'C179.1')
    net(V12, 'R192.2', 'R193.2'); net('AUDX', 'C179.2', 'R194.1'); net('AUDJ', 'R194.2', 'C178.1', 'J115.2'); net('SPA', 'R195.2', 'C177.1', 'J115.13')
    net(GND, 'C177.2', 'C178.2', 'C181.2'); net('AUD', 'R167.1', 'C181.1')
    unsure('Video page read from the 600 dpi scan: A104 is a transistor array, not a CD4050 (A104A base 12, collector 14, emitter 13; A104B collector 11, emitter 10, base pin not printed, 9 assumed; C and D, the sound amplifier, not modelled); '
           'CR108 polarity (anode on the luma node), which A103 buffer feeds which of R180-R183 (36K, 18K, 9.1K, 4700 in the order of the rows), the 10pF coupling capacitor has no reference number on the print (C16X here), '
           'and the parts read from the 300 dpi crops below. The simulator draws the picture from the GTIA model, not from these parts.')
    unsure('RF modulator and sound amplifier (read at 300 dpi): Q106 is NPN, the emitter node RFN joins C169, R188, C173, R190 and C172; A104C is base 4, emitter 3, collector 5 and A104D base 2, emitter 3, collector 1 (the two 3s are one node, with R164 to ground); C181 is taken to ground at its right plate, and the lower of the two 4700 resistors R194 / R195 is R195 by position. The tuning coil L101 and the pairing of the equal parts are by position.')


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
    part('R404', 'R', 2, '1'); net('EXSELB', 'R404.1'); net('PHI2', 'R404.2')
    part('R402', 'R', 2, '2200'); net(V5, 'R402.1'); net('EXSEL', 'R402.2')
    # Z402A -> /GBA, Z403C -> GBA (pull-up R401)
    net('R/W', 'Z402.1'); net('GBAN', 'Z402.3', 'Z403.12')
    net('GBA', 'Z403.11')
    part('R401', 'R', 2, '4700'); net(V5, 'R401.1'); net('GBA', 'R401.2')
    # Z402B RASTIME from phase 1, Z402C /WRITIME from phase 2B
    net('PHI1', 'Z402.9'); net('RASTIME', 'Z402.8'); net('PHIX', 'Z402.10', 'Z402.4')
    net('PHI2', 'Z402.5'); net('WRITIME', 'Z402.6')
    part('R403', 'R', 2, '470'); net('PHI2', 'R403.1')
    for i, (l, a, b) in enumerate((('L401', 'R403.2', 'L401.1'), ('L402', 'L401.2', 'L402.1'), ('L403', 'L402.2', 'L403.1'))):
        part(l, 'L', 2, '22uH')
    net('DLY1', 'R403.2', 'L401.1'); net('DLY2', 'L401.2', 'L402.1'); net('DLY3', 'L402.2', 'L403.1'); net('PHIX', 'L403.2')
    for c, v, n_ in (('C407', '47pF', 'DLY1'), ('C408', '68pF', 'DLY2'), ('C409', '47pF', 'DLY3'), ('C410', '68pF', 'PHIX')):
        part(c, 'C', 2, v); net(n_, c + '.1'); net(GND, c + '.2')
    for c in ('C401', 'C402'):
        part(c, 'C', 2, '.1uF' if c == 'C401' else '.001uF'); net(V5, c + '.1'); net(GND, c + '.2')
    unsure('Personality gates: reread at 200 dpi and now agree with the netlist. Z402A (1 = R/W early, 2 = EXSEL node, 3 = /GBA), Z402B (9 = phase 1, 10 = delayed phase 2, 8 = RASTIME), Z402C (5 = phase 2, 4 = delayed phase 2, 6 = /WRITIME), with 10 and 4 joined and fed from the far end of R403 / L401-L403 / C407-C410. Z403A (1 = decoder Y4, 2 = decoder Y0), Z403B (9 = A15, 10 = phase 2 through R404 1 ohm), Z403C (11 = GBA with R401, 12 = /GBA, 13 = EXSEL node), Z403D (4, 5 = RAM S5 and S4, which of the two goes to which input is not told apart and does not matter for a NAND). Outputs 3, 6, 8 and the EXSEL edge pin are one open-collector node with R402 2200. The personality print labels edge pin 5 as phase 2B, which is also where A0 sits; the motherboard has phase 2 on S, used here.')


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
    net('R/W', z1 + '.5'); net('REF', z1 + '.3', z1 + '.2'); net(selB, z1 + '.13'); net(selA, z1 + '.1')
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
    ram16('_1', 'MS0', 'MS1', 'J104'); ram16('_2', 'MS2', 'MS3', 'J103'); ram16('_3', 'S4G', 'S5G', 'J102')
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
