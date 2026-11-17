#!/usr/bin/env python3
"""c128_board.py - the Commodore 128 board (schematic 310378 rev 7A, the five sheets)

usage: python3 tools/c128_board.py > src/c128_board_data.c

The netlist is typed in from the schematic sheets (processors / MMU / PLA, video / clock / reset, I/O, RAM / ROM)
and the redrawn full schematic, which has readable net labels. Chips are placed as on Jorgen Olsson's rev 7 board
image; the passives are put near the chip they belong to. Sections are kept in separate functions so a sheet can
be checked on its own; notes on what was read where are in tools/c128_notes.md."""
import sys

parts = {}
nets = {}


def part(ref, value, fp, pins):
    parts[ref] = (value, fp, [str(i) for i in range(1, pins + 1)] if isinstance(pins, int) else list(pins))


def net(name, *pins):
    nets.setdefault(name, []).extend(pins)


def dipfp(n, w):
    return 'DIP-%d_W%.2fmm' % (n, w)


A = ['A%d' % i for i in range(16)]
D = ['D%d' % i for i in range(8)]
GND = 'GND'
VCC = '+5V'


def ics():
    """Every IC of the board with its package."""
    for ref, val, n, w in (
            ('U1', '6526 CIA', 40, 15.24), ('U4', '6526 CIA', 40, 15.24), ('U2', '4066', 14, 7.62),
            ('U3', '74LS138', 16, 7.62), ('U5', '6581 SID', 28, 15.24), ('U6', '8502', 40, 15.24),
            ('U7', '8722 MMU', 48, 15.24), ('U8', '74LS08', 14, 7.62), ('U9', '74F32', 14, 7.62),
            ('U10', 'Z80A', 40, 15.24), ('U11', '8721 PLA', 48, 15.24), ('U12', '74LS373', 20, 7.62),
            ('U13', '74LS244', 20, 7.62), ('U14', '74LS257', 16, 7.62), ('U15', '74LS257', 16, 7.62),
            ('U16', '74LS14', 14, 7.62), ('U17', '74ALS373', 20, 7.62), ('U18', '390059-01', 24, 15.24),
            ('U19', '2016', 24, 15.24), ('U20', '4066', 14, 7.62), ('U21', '8564 VIC-IIe', 48, 15.24),
            ('U22', '8563 VDC', 40, 15.24), ('U23', '4416', 18, 7.62), ('U25', '4416', 18, 7.62),
            ('U24', '74LS244', 20, 7.62), ('U26', '74LS257', 16, 7.62), ('U27', '556', 14, 7.62),
            ('U28', '8701', 16, 7.62), ('U29', '7406', 14, 7.62), ('U30', '7406', 14, 7.62),
            ('U31', '74LS00', 14, 7.62), ('U32', '318023-02', 28, 15.24), ('U33', '318018-04', 28, 15.24),
            ('U34', '318019-04', 28, 15.24), ('U35', '318020-05', 28, 15.24), ('U36', 'function ROM', 28, 15.24),
            ('U37', '7406', 14, 7.62), ('U54', '74LS32', 14, 7.62), ('U55', '74F245', 20, 7.62),
            ('U56', '74LS74', 14, 7.62), ('U57', '7407', 14, 7.62), ('U58', '74LS03', 14, 7.62),
            ('U60', '7407', 14, 7.62), ('U61', '74LS08', 14, 7.62), ('U62', '74LS244', 20, 7.62),
            ('U63', '7406', 14, 7.62)):
        part(ref, val, dipfp(n, w), n)
    for i in range(38, 54):
        part('U%d' % i, '4164-15', dipfp(16, 7.62), 16)


# 74xx pin helpers ------------------------------------------------------------------------------------------
def gate(ref, g, a, b, y):
    """Four-gate 14-pin chip (74LS00/08/32/03, 74F32): gate g (1-4) with inputs a, b and output y (net names)."""
    pa, pb, py = {1: (1, 2, 3), 2: (4, 5, 6), 3: (9, 10, 8), 4: (12, 13, 11)}[g]
    if a: net(a, '%s.%d' % (ref, pa))
    if b: net(b, '%s.%d' % (ref, pb))
    if y: net(y, '%s.%d' % (ref, py))


def inv(ref, g, a, y):
    """Hex inverter / buffer (7406, 7407, 74LS14, 74LS04): gate g (1-6) from a to y."""
    pa, py = {1: (1, 2), 2: (3, 4), 3: (5, 6), 4: (9, 8), 5: (11, 10), 6: (13, 12)}[g]
    if a: net(a, '%s.%d' % (ref, pa))
    if y: net(y, '%s.%d' % (ref, py))


def power(ref, vcc, gnd):
    net(VCC, '%s.%d' % (ref, vcc))
    net(GND, '%s.%d' % (ref, gnd))


def pullup(ref, value, n):
    part(ref, value, 'R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal', 2)
    net(VCC, ref + '.1')
    net(n, ref + '.2')


def decouple(ref, vcc='+5V'):
    part(ref, '.1uF', 'C_Disc_D3.0mm_W2.0mm_P2.50mm', 2)
    net(vcc, ref + '.1')
    net(GND, ref + '.2')


# ----------------------------------------------------------------------------------------------------------------
# Sheet 2: the processors, the MMU, the PLA, the Z80's bus glue, the SID, the I/O decoder
# ----------------------------------------------------------------------------------------------------------------
def sheet_cpu():
    # 8502 (U6)
    a8502 = list(range(7, 21)) + [22, 23]
    for i in range(16):
        net(A[i], 'U6.%d' % a8502[i])
    for i in range(8):
        net(D[i], 'U6.%d' % (38 - i))
    net('2MHZ', 'U6.1')
    net('RDY', 'U6.2', 'U61.8')                 # U61C output
    net('GAEC', 'U6.5')
    net('8502R/W', 'U6.39', 'U8.4')
    net('/RES', 'U6.40')
    net('/IRQ', 'U6.3')
    net('/NMI', 'U6.4')
    net('LORAM', 'U6.30')
    net('HIRAM', 'U6.29')
    net('CHAREN', 'U6.28')
    net('CASS_WRT', 'U6.27')
    net('CASS_SENSE', 'U6.26')
    net('CASS_MTR', 'U6.25')
    net('CAPLK_SENSE', 'U6.24')
    power('U6', 6, 21)
    pullup('R27', '4.7K', 'RDY')
    pullup('R10', '3.3K', '8502R/W')
    for n, nm in (('RP7A', 'LORAM'), ('RP7B', 'HIRAM'), ('RP7C', 'CHAREN')):
        pullup(n, '3.3K', nm)

    # Z80A (U10)
    az = [30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 1, 2, 3, 4, 5]
    for i in range(16):
        net(A[i], 'U10.%d' % az[i])
    zd = [14, 15, 12, 8, 7, 9, 10, 13]
    for i in range(8):
        net('ZD%d' % i, 'U10.%d' % zd[i])
    net('/M1', 'U10.27', 'U31.10')
    net('/RD', 'U10.21', 'U12.1')
    net('/WR', 'U10.22', 'U31.1', 'U56.1')
    net('/Z80_IO', 'U10.20')                    # /IORQ, to the PLA's Z80 I/O input
    net('/BUSRQ', 'U10.25', 'U37.8', 'U37.6', 'U60.2')
    net('BUSAK', 'U10.23', 'U37.10', 'U8.1')
    net('Z80PHI', 'U10.6')
    net('/RES', 'U10.26')
    net('+5V', 'U10.24', 'U10.28', 'U10.18', 'U10.11')    # /WAIT, /RFSH, /HALT left open on the board; held high
    net('/Z80NMI', 'U10.17')
    net('/Z80INT', 'U10.16')
    pullup('R34', '10K', '/Z80NMI')
    pullup('RP2I', '1K', '/Z80INT')
    net(GND, 'U10.29')
    pullup('R8', '3.3K', '/WR')
    pullup('RP7D', '3.3K', '/Z80_IO')
    pullup('RP7E', '3.3K', '/M1')
    pullup('RP7F', '3.3K', '/RD')
    pullup('RP7G', '3.3K', '/BUSRQ')
    # the Z80's data latches: U12 puts the system data on the Z80's bus while it reads, U13 puts its data on the
    # system bus while it writes
    zl = {1: ('3', '2'), 2: ('4', '5'), 3: ('7', '6'), 4: ('8', '9'), 5: ('13', '12'), 6: ('14', '15'),
          7: ('17', '16'), 8: ('18', '19')}
    for i in range(8):
        d_, q_ = zl[i + 1]
        net(D[i], 'U12.' + d_)
        net('ZD%d' % i, 'U12.' + q_)
    net('D_1MHZ', 'U12.11', 'U56.3')
    power('U12', 20, 10)
    for i, (a_, y_) in enumerate(((17, 3), (15, 5), (13, 7), (11, 9))):      # 2A -> 2Y: ZD0..3 -> D0..3
        net('ZD%d' % i, 'U13.%d' % a_)
        net(D[i], 'U13.%d' % y_)
    for i, (a_, y_) in zip(range(4, 8), ((8, 12), (6, 14), (4, 16), (2, 18))):  # 1A -> 1Y: ZD4..7 -> D4..7
        net('ZD%d' % i, 'U13.%d' % a_)
        net(D[i], 'U13.%d' % y_)
    net('/ZWRT', 'U13.1', 'U13.19', 'U31.11', 'U8.5')
    power('U13', 20, 10)
    # write timing: U56A (the Z80's /WR clocked by the 1 MHz), U31 gates
    net('+5V', 'U56.2', 'U56.4')
    net('OLD_WR', 'U56.5', 'U31.12')
    net('OLD_RD', 'U56.6', 'U31.4')
    net('GAEC', 'U31.13', 'U61.3')
    net('GWR', 'U31.3')
    net('U31B', 'U31.6', 'U31.2')
    net(VCC, 'U31.5')
    power('U56', 14, 7)
    power('U31', 14, 7)
    # bus request / acknowledge, halting the 8502
    gate('U61', 1, 'DMA', 'AEC', 'GAEC')
    net('BA', 'U61.4')
    net('/Z80EN', 'U61.5')
    net('/HALT8502', 'U61.6', 'U61.9')
    net('BUSFREE', 'U61.11', 'U61.10')
    net('DMA', 'U61.12')
    net('BUSACK', 'U61.13', 'U37.11')
    power('U61', 14, 7)
    net('BA', 'U60.1')
    net('/Z80EN', 'U37.9')
    net('/AEC', 'U8.2')
    net('U8A', 'U8.3', 'U37.5')
    # U8B: F_R/W = 8502 R/W and /ZWRT; U57A buffers it onto R/W
    net('F_R/W', 'U8.6', 'U57.1')
    net('R/W', 'U57.2', 'U1.22', 'U4.22', 'U5.7', 'U7.32')
    pullup('RP1B', '1K', 'R/W')
    power('U8', 14, 7)
    power('U37', 14, 7)
    power('U57', 14, 7)
    power('U60', 14, 7)
    # the Z80's clock: from the VIC through Q6 and two 7407 buffers
    # (Q6 is a level shifter to the +12V for the Z80A's clock input)
    # MMU (U7)
    net('+5V', 'U7.1')
    net(GND, 'U7.34')
    net('/RES', 'U7.2')
    for i, p_ in enumerate((10, 9, 8, 7, 6, 5, 4, 3)):
        net('TA%d' % (8 + i), 'U7.%d' % p_)
    net('CAS1', 'U7.11', 'U9.5')
    net('CAS0', 'U7.12', 'U9.2')
    net('I/O_SEL', 'U7.13', 'U11.16')
    net('ROMBANKHI', 'U7.14', 'U11.17')
    net('ROMBANKLO', 'U7.15', 'U11.18')
    net('GAEC', 'U7.16')
    net('MUX', 'U7.17')
    for i in range(4):
        net(A[i], 'U7.%d' % (18 + i))
    net('U54B', 'U7.22', 'U54.11')
    net('U54D', 'U7.23', 'U54.6')
    for i in range(8):
        net(A[8 + i], 'U7.%d' % (24 + i))
    net('2MHZ', 'U7.33')
    for i in range(8):
        net(D[i], 'U7.%d' % (35 + i))
    net('/Z80EN', 'U7.43')
    net('/FSDIR', 'U7.44')
    net('GAME', 'U7.45')
    net('EXROM', 'U7.46')
    net('128/64', 'U7.47', 'U11.15')
    net('40/80', 'U7.48')
    net('A4', 'U54.13')
    net('A5', 'U54.12')
    net('A6', 'U54.5')
    net('A7', 'U54.4')
    power('U54', 14, 7)
    for i, nm in enumerate(('TA12', 'TA13', 'TA14', 'TA15', '/Z80EN')):
        pullup('RP8%s' % 'ABCDE'[i], '3.3K', nm)
    decouple('C7'); decouple('C77')
    # PLA (U11)
    for i, p_ in enumerate((6, 5, 4, 3, 2, 1)):
        net(A[10 + i], 'U11.%d' % p_)
    net('VICFIX', 'U11.7', 'J2.1')
    net(GND, 'J2.2', 'U11.8')
    net('AEC', 'U11.9')
    net('R/W', 'U11.10')
    net('GAME', 'U11.11')
    net('EXROM', 'U11.12')
    net('BUSACK', 'U11.13')
    net('/Z80_IO', 'U11.14')
    net('VMA4', 'U11.19')
    net('VMA5', 'U11.20')
    net('BA', 'U11.21')
    net('LORAM', 'U11.22')
    net('HIRAM', 'U11.23')
    net(GND, 'U11.24')
    net('CHAREN', 'U11.25')
    net('VA14', 'U11.26')
    net('128/256', 'U11.27', 'J3.1', 'R33.2')
    net(GND, 'J3.2')
    pullup('R33', '10K', '128/256')
    net('ROML', 'U11.30')
    net('ROMH', 'U11.31')
    net('CLRBNK', 'U11.32')
    net('FROM1', 'U11.33')
    net('ROM4', 'U11.34')
    net('ROM3', 'U11.35')
    net('ROM2', 'U11.36')
    net('ROM1', 'U11.37')
    net('IOCS', 'U11.38', 'U3.5')
    net('DWE', 'U11.40')
    net('CASENB', 'U11.41', 'U9.1', 'U9.4')
    net('VIC', 'U11.42')
    net('IOACC', 'U11.43')
    net('GWE', 'U11.44')
    net('COLORAM', 'U11.45')
    net('CHAROM', 'U11.46')
    net('CAS', 'U11.47')
    net(VCC, 'U11.48')
    decouple('C2'); decouple('C11')
    # RAM CAS enables
    net('GCAS0', 'U9.3')
    net('GCAS1', 'U9.6')
    power('U9', 14, 7)
    # the DRAM address multiplexers: the processors' address, or the MMU's translated one, to the DRAMs
    m = {  # chip: (A-input pin, B-input pin, Y pin) per gate, with the MMU's TA line, the CPU address line, the MA line
        'U14': ((2, 3, 4, 'TA15', 'A7', 'MA7'), (5, 6, 7, 'TA14', 'A6', 'MA6'), (11, 10, 9, 'TA13', 'A5', 'MA5'),
                (14, 13, 12, 'TA12', 'A4', 'MA4')),
        'U15': ((2, 3, 4, 'TA11', 'A3', 'MA3'), (5, 6, 7, 'TA10', 'A2', 'MA2'), (11, 10, 9, 'TA9', 'A1', 'MA1'),
                (14, 13, 12, 'TA8', 'A0', 'MA0')),
    }
    for ref, gl in m.items():
        for pa, pb, py, ta, ca, ma in gl:
            net(ta, '%s.%d' % (ref, pa))
            net(ca, '%s.%d' % (ref, pb))
            net(ma + 'X', '%s.%d' % (ref, py))
        net('MUX', ref + '.1')
        net('AEC', ref + '.15')
        power(ref, 16, 8)
    decouple('C14'); decouple('C15')
    # SID (U5)
    for i in range(5):
        net(A[i], 'U5.%d' % (9 + i))
    for i in range(8):
        net(D[i], 'U5.%d' % (15 + i))
    net('EXT_AUDIO', 'U5.26')
    net('POTX', 'U5.24')
    net('POTY', 'U5.23')
    net('SID_AUDIO', 'U5.27')
    net('1MHZ', 'U5.6')
    net('/RES', 'U5.5')
    net('/SID', 'U5.8', 'U3.15')
    net('+9V/+12V', 'U5.25')
    net(VCC, 'U5.28')
    net(GND, 'U5.14')
    net('SID_C1A', 'U5.1', 'C83.1')
    net('SID_C1B', 'U5.2', 'C83.2')
    net('SID_C2A', 'U5.3', 'C82.1')
    net('SID_C2B', 'U5.4', 'C82.2')
    for ref, val, n1, n2 in (('C83', '470pF', 'SID_C1A', 'SID_C1B'), ('C82', '470pF', 'SID_C2A', 'SID_C2B')):
        part(ref, val, 'C_Disc_D3.0mm_W2.0mm_P2.50mm', 2)
    decouple('C5'); decouple('C86', '+9V/+12V')
    # I/O decoder (U3) and its enable gate U31C
    net('A8', 'U3.1')
    net('A9', 'U3.2')
    net('A11', 'U3.3')
    net('/G3', 'U3.4', 'U31.8')
    net('A10', 'U31.9')
    net(VCC, 'U3.6')
    net('/SID', 'U3.15')
    net('CS8563', 'U3.13')
    net('CIA1', 'U3.11')
    net('CIA2', 'U3.10')
    net('IO1', 'U3.9')
    net('IO2', 'U3.7')
    net('NC_U3_14', 'U3.14')
    net('NC_U3_12', 'U3.12')
    power('U3', 16, 8)
    decouple('C3')
    part('J2', 'N.C.', 'Jumper_2', 2)
    part('J3', 'open = 128K ROM', 'Jumper_2', 2)


def sheet_mem():
    """Sheet 5: the DRAM, the ROMs, the address pull-ups."""
    for i in range(8):                          # the 33 ohm packs RP3 / RP4 between the multiplexers and the DRAM address
        part('R_MA%d' % i, '33', 'R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal', 2)
        net('MA%dX' % i, 'R_MA%d.1' % i)
        net('MA%d' % i, 'R_MA%d.2' % i)
    ma = {0: 5, 1: 7, 2: 6, 3: 12, 4: 11, 5: 10, 6: 13, 7: 9}
    for k in range(16):
        ref = 'U%d' % (38 + k)
        bit = k % 8
        net('RAS', ref + '.4')
        net('DWE', ref + '.3')
        net('RAMCAS0' if k < 8 else 'RAMCAS1', ref + '.15')
        for a_, p_ in ma.items():
            net('MA%d' % a_, '%s.%d' % (ref, p_))
        net(D[bit], ref + '.2', ref + '.14')
        net(VCC, ref + '.8')
        net(GND, ref + '.16')
        decouple('C%d' % (38 + k))
    gate('U9', 3, 'GCAS0', 'CAS', 'RAMCAS0X')
    gate('U9', 4, 'CAS', 'GCAS1', 'RAMCAS1X')
    for ref, a_, b_ in (('R29', 'RAMCAS0X', 'RAMCAS0'), ('R30', 'RAMCAS1X', 'RAMCAS1')):
        part(ref, '68', 'R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal', 2)
        net(a_, ref + '.1')
        net(b_, ref + '.2')
    # ROMs: address pins A0-A7 3-10 (reversed), A8 25, A9 24, A10 21, A11 23, A12 2, A13 26, A14/CS 27
    rd = [11, 12, 13, 15, 16, 17, 18, 19]
    ra = {0: 10, 1: 9, 2: 8, 3: 7, 4: 6, 5: 5, 6: 4, 7: 3, 8: 25, 9: 24, 10: 21, 11: 23}
    for ref, oe in (('U32', 'ROM1'), ('U33', 'ROM2'), ('U34', 'ROM3'), ('U35', 'ROM4'), ('U36', 'FROM1')):
        for a_, p_ in ra.items():
            net(A[a_], '%s.%d' % (ref, p_))
        for i in range(8):
            net(D[i], '%s.%d' % (ref, rd[i]))
        net(oe, ref + '.22')
        net(GND, ref + '.20')
        net(VCC, ref + '.1')                    # pin 1 (EPROM PGM) held high
        power(ref, 28, 14)
    net('TA12', 'U32.2', 'U35.2')
    net('A12', 'U33.2', 'U34.2', 'U36.2')
    net('A13/A14', 'U32.26', 'U8.11')
    net('A13', 'U8.12', 'U33.26', 'U34.26', 'U35.26', 'U36.26')
    net('TA14', 'U8.13')
    net('U32_A14', 'U32.27', 'J6.1')
    net('128/64', 'J6.2')
    net('U34_A14', 'U34.27', 'J4.1', 'R36.2')
    net('A14', 'J4.2', 'U36.27')
    net(VCC, 'U33.27', 'U35.27')
    part('J4', 'A14', 'Jumper_2', 2)
    part('J6', '128/64', 'Jumper_2', 2)
    pullup('R36', '10K', 'U34_A14')
    pullup('R38', '10K', 'U32_A14')
    for i in range(5):
        decouple('C%d' % (32 + i))
    # address bus pull-ups (RP9: A0-A7, RP10: A8-A15) and on the DRAM address lines (RP6)
    for ref, base in (('RP9', 0), ('RP10', 8)):
        for i in range(8):
            pullup('%s_%d' % (ref, i + 1), '10K', A[base + i])
    for i in range(8):
        pullup('RP6_%d' % (i + 1), '1K', 'MA%d' % i)
    # U62: the translated address onto the bus for a DMA device
    for ta, a_, pa, py in (('TA15', 'A15', 2, 18), ('TA14', 'A14', 4, 16), ('TA13', 'A13', 6, 14), ('TA12', 'A12', 8, 12),
                           ('TA11', 'A11', 11, 9), ('TA10', 'A10', 13, 7), ('TA9', 'A9', 15, 5), ('TA8', 'A8', 17, 3)):
        net(ta, 'U62.%d' % pa)
        net(a_, 'U62.%d' % py)
    net('/DMA', 'U62.1', 'U62.19')
    power('U62', 20, 10)


def sheet_video():
    """Sheet 3 (part): the video address latch/buffer, colour RAM and its data gate. Read so far: U17, U55, U19, U20,
    U56B. Still to type: U21 8564, U28 8701, U18, U26, U22/U23/U25 (VDC), U24, clock and reset circuits."""
    # U55 (74F245): the CPU address A0-A7 onto the video side (SA0-SA7) when /DMA
    for i, (pa, pb) in enumerate(((3, 17), (5, 15), (7, 13), (9, 11), (8, 12), (6, 14), (4, 16), (2, 18))):
        net(A[i], 'U55.%d' % pa)
        net('SA%d' % i, 'U55.%d' % pb)
    net('/DMA', 'U55.1')
    net('/AEC', 'U55.19')
    power('U55', 20, 10)
    # U17 (74ALS373): the VIC's address VMA0-5, VA6, VA7 latched onto SA0-SA7 while it owns the bus
    din = [3, 4, 7, 8, 13, 14, 17, 18]
    qout = [2, 5, 6, 9, 12, 15, 16, 19]
    src = ['VMA0', 'VMA1', 'VMA2', 'VMA3', 'VMA4', 'VMA5', 'VA6', 'VA7']
    for i in range(8):
        net(src[i], 'U17.%d' % din[i])
        net('SA%d' % i, 'U17.%d' % qout[i])
    net('AEC', 'U17.1')
    net('U17_E', 'U17.11')
    power('U17', 20, 10)
    # U19 (2016): the colour RAM, 2K x 8, addressed SA0-SA7, TA8, TA9 and CLRBNK
    sa = {0: 8, 1: 7, 2: 6, 3: 5, 4: 4, 5: 3, 6: 2, 7: 1}
    for i, pn in sa.items():
        net('SA%d' % i, 'U19.%d' % pn)
    net('TA8', 'U19.23')
    net('TA9', 'U19.22')
    net('CLRBNK', 'U19.19')
    for i, pn in enumerate((9, 10, 11, 13)):
        net('D%d_VIC' % (8 + i), 'U19.%d' % pn)
    for pn, rp in ((14, 'RP7_V2'), (15, 'RP7_V3'), (16, 'RP7_V4'), (17, 'RP7_V5')):
        pullup(rp, '3.3K', 'U19_%d' % pn)
        net('U19_%d' % pn, 'U19.%d' % pn)
    net('U19_CS', 'U19.18', 'U19.20')
    net('U19_WE', 'U19.21', 'U56.9')
    net('U56B_Q', 'U56.9')
    power('U19', 24, 12)
    # U20 (4066): colour RAM low nibble between the VIC's D8-D11 and the system D0-D3
    for i, (pa, pb) in enumerate(((1, 2), (4, 3), (8, 9), (11, 10))):
        net('D%d_VIC' % (8 + i), 'U20.%d' % pa)
        net(D[i], 'U20.%d' % pb)
    net('U20_EN', 'U20.13', 'U20.5', 'U20.6', 'U20.12')
    power('U20', 14, 7)
    # U56B (second half of the 74LS74): colour RAM write strobe
    net('+5V', 'U56.10', 'U56.13')
    net('U56B_D', 'U56.12')
    net('U56B_CLK', 'U56.11')
    for r in ('C17', 'C55', 'C19', 'C20'):
        decouple(r)


def build():
    ics()
    sheet_cpu()
    sheet_mem()
    sheet_video()


if __name__ == '__main__':
    build()
    print('%d parts, %d nets' % (len(parts), len(nets)), file=sys.stderr)
