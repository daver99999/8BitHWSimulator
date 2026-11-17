#!/usr/bin/env python3
"""plus4_board.py - the Commodore Plus/4 board (schematic 310164 sheets 1-4, rev C)

usage: python3 tools/plus4_board.py > src/plus4_board_data.c

The netlist is typed in below from the four sheets of the schematic (no KiCad project exists). The chips and the
connectors are put where the layout drawing of the service manual (PN-314001-04, two halves) has them, kept
apart by hand since the drawing isn't to scale; the passives are put in free places next to the chip they are
connected to, not where the board has them."""
import math
import sys

PX = 1.0               # positions below are in millimetres
OUTLINE = (0.0, 0.0, 340.0, 150.0)

parts = {}
nets = {}          # net name -> list of 'REF.PAD'


def part(ref, value, fp, pins):
    parts[ref] = (value, fp, [str(i) for i in range(1, pins + 1)] if isinstance(pins, int) else list(pins))


def net(name, *pins):
    nets.setdefault(name, []).extend(pins)


def dipfp(n, w):
    return 'DIP-%d_W%.2fmm' % (n, w)


# ICs
part('U1', '7360 TED', dipfp(48, 15.24), 48)
part('U2', '8501', dipfp(40, 15.24), 40)
part('U3', '6551A', dipfp(28, 15.24), 28)
part('U4', '74LS08', dipfp(14, 7.62), 14)
part('U5', '6529B', dipfp(20, 7.62), 20)
part('U6', '74LS04', dipfp(14, 7.62), 14)
part('U7', '7406', dipfp(14, 7.62), 14)
part('U8', '555', dipfp(8, 7.62), 8)
part('U9', '74LS257', dipfp(16, 7.62), 16)
part('U10', '74LS257', dipfp(16, 7.62), 16)
for i in range(11, 19):
    part('U%d' % i, '4164-15', dipfp(16, 7.62), 16)
part('U19', '7700-010', dipfp(28, 15.24), 28)
part('U20', '74LS139', dipfp(16, 7.62), 16)
part('U21', '74LS175', dipfp(16, 7.62), 16)
part('U22', '74LS27', dipfp(14, 7.62), 14)
part('U23', '318006-01', dipfp(28, 15.24), 28)
part('U24', '318005-05', dipfp(28, 15.24), 28)
part('U25', '317053-01', dipfp(28, 15.24), 28)
part('U26', '317054-01', dipfp(28, 15.24), 28)
part('U27', '6529B', dipfp(20, 7.62), 20)

# Connectors
EXP = ['1', '2', '3', '4', '5', '6', '7', '8', '9', '10', '11', '12', '13', '14', '15', '16', '17', '18', '19', '20',
       '21', '22', '23', '24', '25', 'A', 'B', 'C', 'D', 'E', 'F', 'H', 'J', 'K', 'L', 'M', 'N', 'P', 'R', 'S', 'T',
       'U', 'V', 'W', 'X', 'Y', 'Z', 'AA', 'BB', 'CC']
USR = ['1', '2', '3', '4', '5', '6', '7', '8', '9', '10', '11', '12', 'A', 'B', 'C', 'D', 'E', 'F', 'H', 'J', 'K', 'L',
       'M', 'N']
part('CN1', 'Power', 'DIN-7', ['1', '2', '3', '4'])
part('CN2', 'Serial bus', 'DIN-6', 6)
part('CN3', 'Cassette', 'DIN-7', 7)
part('CN4', 'Expansion', 'PLUS4_EXPANSION_2x25', EXP)
part('CN5', 'Joystick 0', 'DIN-8', 8)
part('CN6', 'Joystick 1', 'DIN-8', 8)
part('CN7', 'Audio/Video', 'DIN-8', 8)
part('CN8', 'Keyboard', 'PinHeader_1x18_P2.54mm_Vertical', 18)
part('CN9', 'User port', 'PLUS4_USERPORT_2x12', USR)
part('M1', 'RF modulator', 'RF_MODULATOR', ['1', '2', '3', '4', '5', '6', '7', '8', 'CASE'])

R = {'R1': '4.7K', 'R2': '10K', 'R3': '470', 'R4': '220', 'R5': '18K', 'R6': '1.5K', 'R7': '470', 'R8': '100K',
     'R9': '1K', 'R10': '1K', 'R11': '12K', 'R12': '10K', 'R13': '1K', 'R14': '240', 'R15': '240', 'R16': '100K',
     'R17': '1.5K', 'R18': '47K', 'R19': '100K', 'R20': '3K', 'R21': '1K', 'R22': '1K', 'R23': '1K', 'R24': '1K',
     'R25': '1K', 'R26': '10K', 'R27': '10K'}
for r, v in R.items():
    part(r, v, 'R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal', 2)
C = {'C1': '100uF', 'C2': '.1uF', 'C3': '.22uF', 'C4': '.22uF', 'C5': '.22uF', 'C6': '.22uF', 'C7': '2200uF',
     'C8': '10uF', 'C9': '.1uF', 'C10': '4-20pF', 'C11': '33pF', 'C12': '220pF', 'C13': '150pF', 'C14': '.01uF',
     'C15': '.1uF', 'C16': '.1uF', 'C17': '10uF', 'C18': '10uF', 'C19': '2700pF', 'C20': '10uF', 'C21': '.1uF',
     'C22': '10uF', 'C23': '1uF', 'C24': '.1uF', 'C25': '.1uF', 'C26': '.1uF', 'C27': '10uF', 'C28': '.1uF',
     'C29': '.1uF', 'C30': '10uF', 'C31': '.1uF', 'C32': '.1uF', 'C41': '.1uF', 'C42': '.1uF', 'C43': '.1uF',
     'C44': '.1uF', 'C45': '.1uF', 'C46': '.1uF', 'C47': '.1uF', 'C48': '.1uF', 'C49': '.1uF', 'C50': '.1uF',
     'C51': '.1uF', 'C100': '.1uF'}
for i in range(33, 41):
    C['C%d' % i] = '.22uF'
for c, v in C.items():
    part(c, v, 'C_Disc_D4.7mm_W2.5mm_P5.00mm', 2)
part('RP1', '3.3K', 'RP_ARRAY_SIP6', 6)
part('RP2', '3.3K', 'RP_ARRAY_SIP6', 6)
part('RP3', '68 isolated', 'RP_ISOLATED_SIP8', 8)
part('RP4', '68 isolated', 'RP_ISOLATED_SIP8', 8)
part('CR1', 'BA 120', 'Bridge_Round', 4)
part('CR2', '6.8V', 'D_DO-35_P7.62mm_Horizontal', 2)
for i in range(11, 21):
    part('CR%d' % i, '1N914', 'D_DO-35_P7.62mm_Horizontal', 2)
for q in ('Q1', 'Q2', 'Q3'):
    part(q, '2SC1815', 'Q_NPN_ECB', 3)
part('Q4', '2SD880', 'Q_NPN_ECB', 3)
part('Y1', '14.31818 MHz', 'Crystal_HC18-U_Vertical', 2)
part('Y2', '1.8432 MHz', 'Crystal_HC18-U_Vertical', 2)
part('F1', '1.5A', 'Fuseholder_Cylinder-5x20mm', 2)
part('SW1', 'POWER +5V', 'SW_Slide', 2)
part('SW1B', 'POWER 9VAC', 'SW_Slide', 2)
part('SW2', 'RESET', 'SW_Push', 2)
part('L1', 'EMI filter', 'L_Axial_L7.0mm_D3.3mm_P5.08mm_Vertical', 2)
part('L2A', 'LINE FILTER', 'L_Axial_L7.0mm_D3.3mm_P5.08mm_Vertical', 2)
part('L2B', 'LINE FILTER', 'L_Axial_L7.0mm_D3.3mm_P5.08mm_Vertical', 2)
part('L4', '1.2uH', 'L_Axial_L7.0mm_D3.3mm_P5.08mm_Vertical', 2)
part('L5', '1.2uH', 'L_Axial_L7.0mm_D3.3mm_P5.08mm_Vertical', 2)
part('J8', 'N.O.', 'Jumper_2', 2)
part('J9', 'N.C.', 'Jumper_2', 2)
FB = [2] + list(range(3, 27)) + list(range(28, 66))
for n in FB:
    part('FB%d' % n, 'ferrite', 'R_Axial_Link_FB', 2)
for e in ('EM1', 'EM2'):
    part(e, 'EMI', 'EMI_T-FILTER', 3)

# ----------------------------------------------------------------------------------------------------------------
# Nets
# ----------------------------------------------------------------------------------------------------------------
A = ['A%d' % i for i in range(16)]
D = ['D%d' % i for i in range(8)]

# TED (U1): A0-A2 pins 3,2,1; A3-A15 pins 48..36; D0-D7 25-32; K0-K7 15-22
ted_a = {0: 3, 1: 2, 2: 1}
for i in range(3, 16):
    ted_a[i] = 51 - i
for i in range(16):
    net(A[i], 'U1.%d' % ted_a[i])
for i in range(8):
    net(D[i], 'U1.%d' % (25 + i), 'U2.%d' % (38 - i), 'U27.%d' % (18 - i), 'U5.%d' % (18 - i), 'U3.%d' % (18 + i))
    net('K%d' % i, 'U1.%d' % (15 + i))
for i in range(16):
    net(A[i], 'U2.%d' % ([6 + k for k in range(14)] + [21, 22])[i])
net('+5V', 'U1.4', 'U2.5', 'FB18.1', 'FB18.2', 'C29.1', 'C30.1', 'C26.1', 'C27.1')
net('GND', 'U1.24', 'U2.20', 'C29.2', 'C30.2', 'C26.2', 'C27.2')
net('R/W', 'U1.7', 'U2.39', 'U27.1', 'U5.1', 'U3.28', 'U22.2', 'CN4.5')
net('/IRQ', 'U1.8', 'U2.3', 'R20.1', 'U3.26', 'CN4.4')
net('+5V', 'R20.2')
net('MUX', 'U1.9', 'U2.23', 'U9.1', 'U10.1', 'U19.27', 'CN4.12')
net('/RAS', 'U1.10', 'U19.20', 'FB15.1')
net('/CAS', 'U1.11', 'FB17.1')
net('PHI0', 'U1.12', 'U2.1', 'U19.8', 'FB16.1')
net('CLKIN', 'U1.14', 'R7.1', 'Y1.1', 'Q2.2')
net('BA', 'U1.34', 'U2.2', 'CN4.13')
net('AEC', 'U1.35', 'U2.4', 'CN4.22')
net('/CS0', 'U1.5', 'U20.1', 'CN4.10')
net('/CS1', 'U1.6', 'U20.15', 'CN4.9')
net('COLOR_T', 'U1.13', 'FB21.1')
net('LUM_T', 'U1.23', 'FB20.1')
net('SND_T', 'U1.33', 'FB19.1')

# power on reset: the 555 (U8) and the 7406 (U7: 13>12 /RESET, 1>2 /BRESET)
net('PWR', 'U8.3', 'U7.13', 'U7.1')
net('/RESET', 'U7.12', 'U2.40', 'R21.2', 'U3.4', 'U21.1')
net('+5V', 'R21.1', 'R22.1')
net('/BRESET', 'U7.2', 'R22.2', 'CN4.C', 'FB23.1', 'FB62.1')
net('555_THR', 'U8.6', 'U8.7', 'R18.2', 'C22.1')
net('555_TRG', 'U8.2', 'R19.2', 'C23.1', 'SW2.1')
net('+5V', 'U8.4', 'U8.8', 'R18.1', 'R19.1', 'C24.1', 'U7.14', 'C25.1')
net('GND', 'U8.1', 'C22.2', 'C23.2', 'SW2.2', 'C24.2', 'U7.7', 'C25.2')

# serial bus (CPU port bits), cassette port
net('SER_DATA_OUT', 'U2.30', 'U7.9')
net('SER_CLK_OUT', 'U2.29', 'U7.5')
net('SER_ATN_OUT', 'U2.28', 'U7.11')
net('SER_DATA', 'U7.8', 'R25.1', 'FB24.1', 'U2.24', 'J8.2')
net('SER_CLK', 'U7.6', 'R24.1', 'FB25.1', 'U2.25', 'FB12.1')        # P6; the cassette write is this line
net('SER_ATN', 'U7.10', 'R23.1', 'FB26.1', 'FB63.1')
net('+5V', 'R23.2', 'R24.2', 'R25.2')
net('CN2_3', 'FB26.2', 'CN2.3')
net('CN2_4', 'FB25.2', 'CN2.4')
net('CN2_5', 'FB24.2', 'CN2.5')
net('CN2_6', 'FB23.2', 'CN2.6')
net('CN2_2', 'FB22.2', 'CN2.2')
net('GND', 'FB22.1')
net('CST_RD', 'U2.26', 'FB11.1')
net('CST_MTR', 'U2.27', 'U7.3')
net('CST_SENSE', 'FB13.1', 'J8.1', 'J9.1')
net('USR_P2', 'J9.2', 'U5.4', 'CN9.4')
net('MTR_B', 'U7.4', 'Q4.3', 'R17.2', 'CR2.1')
net('MTR', 'Q4.1', 'FB14.1')
net('UNREG', 'Q4.2', 'R17.1')
net('GND', 'CR2.2')
net('CN3_1', 'FB10.2', 'CN3.1', 'CN3.7')
net('GND', 'FB10.1')
net('CN3_2', 'FB9.2', 'CN3.2')
net('+5V', 'FB9.1')
net('CN3_3', 'FB14.2', 'CN3.3')
net('CN3_4', 'FB11.2', 'CN3.4')
net('CN3_5', 'FB12.2', 'CN3.5')
net('CN3_6', 'FB13.2', 'CN3.6')

# keyboard: 6529B U27 ports P0-P7 are the rows (FB41-48), the TED's K0-K7 the columns (FB49-56)
row_pin = [15, 18, 7, 12, 11, 10, 5, 1]
col_pin = [17, 14, 13, 16, 6, 8, 9, 2]
net('/KEYPORT', 'U27.19', 'U19.12')
net('+5V', 'U27.20', 'C41.1')
net('GND', 'U27.10', 'C41.2')
for i in range(8):
    net('ROW%d' % i, 'U27.%d' % (2 + i), 'FB%d.1' % (41 + i))
    net('CN8_%d' % row_pin[i], 'FB%d.2' % (41 + i), 'CN8.%d' % row_pin[i])
    net('K%d' % i, 'FB%d.1' % (49 + i))
    net('CN8_%d' % col_pin[i], 'FB%d.2' % (49 + i), 'CN8.%d' % col_pin[i])
net('GND', 'CN8.3')
net('+5V', 'FB57.1')
net('CN8_4', 'FB57.2', 'CN8.4')

# joysticks: CN5 (JOY 0) pins 1 2 3 4 6 (up down left right fire), CN6 (JOY 1), the diodes CR11-15 / CR16-20, the
# pull-ups RP1 / RP2; the selects are D2 and D1 through the two gates of the 74LS08 U4
jfb = [[28, 29, 30, 31, 32], [33, 34, 35, 36, 37]]
for jn, (cn, rp, dio, sel, em) in enumerate([('CN5', 'RP1', [11, 12, 13, 14, 15], 'U4.6', 'EM1'),
                                              ('CN6', 'RP2', [16, 17, 18, 19, 20], 'U4.8', 'EM2')]):
    for k, jp in enumerate([1, 2, 3, 4, 6]):
        n = 'J%dP%d' % (jn, jp)
        net(n + 'C', 'FB%d.1' % jfb[jn][k], 'CR%d.1' % dio[k], '%s.%d' % (rp, [2, 6, 3, 5, 4][k]))
        net(n + 'J', 'FB%d.2' % jfb[jn][k], '%s.%d' % (cn, jp))
    net('+5V', '%s.1' % rp)
    net('J%dSEL' % jn, sel, '%s.3' % em)
    net('J%dSEL_C' % jn, '%s.1' % em, '%s.8' % cn)
    net('GND', '%s.2' % em)
net('+5V', 'FB2.1', 'FB38.1')
net('J0P5', 'FB2.2', 'CN5.5')
net('J1P5', 'FB38.2', 'CN6.5')
net('GND', 'FB39.1', 'FB40.1')
net('J0P7', 'FB39.2', 'CN5.7')
net('J1P7', 'FB40.2', 'CN6.7')
for k, d in enumerate([11, 12, 13, 14, 15]):
    net('K%d' % [0, 1, 2, 3, 6][k], 'CR%d.2' % d)
for k, d in enumerate([16, 17, 18, 19, 20]):
    net('K%d' % [0, 1, 2, 3, 7][k], 'CR%d.2' % d)
net('D2', 'U4.5')
net('D1', 'U4.9')
net('+5V', 'U4.4', 'U4.10', 'U4.14', 'C28.1')
net('GND', 'U4.7', 'C28.2', 'U4.1', 'U4.2', 'U4.12', 'U4.13')

# memory: ROMs (23128 in all four sockets: jumpers J1-J6 in position A), the 4164s and the address multiplexers
rom_a = [10, 9, 8, 7, 6, 5, 4, 3, 25, 24, 21, 23, 2, 26]
rom_d = [11, 12, 13, 15, 16, 17, 18, 19]
for r in ('U23', 'U24', 'U25', 'U26'):
    for i in range(14):
        net(A[i], '%s.%d' % (r, rom_a[i]))
    for i in range(8):
        net(D[i], '%s.%d' % (r, rom_d[i]))
    net('+5V', '%s.28' % r, '%s.20' % r, '%s.27' % r)
    net('GND', '%s.14' % r)
net('/BASIC_CS', 'U23.22', 'U20.4')
net('/FUNCLOW_CS', 'U25.22', 'U20.5')
net('/KERNAL_CS', 'U24.22', 'U20.12')
net('/FUNCHIGH_CS', 'U26.22', 'U20.11')
net('+5V', 'C45.1', 'C47.1', 'C48.1', 'C51.1')
net('GND', 'C45.2', 'C47.2', 'C48.2', 'C51.2')
ram_a = [5, 7, 6, 12, 11, 10, 13, 9]          # the 4164's address pins A0-A7
for k in range(8):
    r = 'U%d' % (11 + k)
    net(D[k], '%s.2' % r, '%s.14' % r)
    net('R/W', '%s.3' % r)
    net('/RAS', '%s.4' % r)
    net('/CAS', '%s.15' % r)
    net('+5V', '%s.8' % r, 'C%d.1' % (33 + k))
    net('GND', '%s.16' % r, 'C%d.2' % (33 + k))
# multiplexer outputs, bit 0-7 of the DRAM address: U9.Y0 U10.Y0 U10.Y3 U10.Y1 U9.Y2 U9.Y1 U9.Y3 U10.Y2
mux_out = ['U9.4', 'U10.4', 'U10.12', 'U10.7', 'U9.9', 'U9.7', 'U9.12', 'U10.9']
for i in range(8):
    rp = 'RP3' if i < 4 else 'RP4'
    j = i % 4
    net('MA%d_M' % i, mux_out[i], '%s.%d' % (rp, 2 * j + 1))
    net('MA%d' % i, '%s.%d' % (rp, 2 * j + 2), *['U%d.%d' % (11 + k, ram_a[i]) for k in range(8)])
# inputs: B (selected while MUX is high: the row, A0-A7) and A (MUX low: the column)
for pin, a in [('U9.3', 0), ('U9.6', 5), ('U9.10', 4), ('U9.13', 6), ('U10.3', 1), ('U10.6', 3), ('U10.10', 7),
               ('U10.13', 2)]:
    net(A[a], pin)
for pin, a in [('U9.2', 14), ('U9.5', 12), ('U9.11', 11), ('U9.14', 13), ('U10.2', 8), ('U10.5', 10), ('U10.11', 15),
               ('U10.14', 9)]:
    net(A[a], pin)
net('GND', 'U9.15', 'U9.8', 'U10.15', 'U10.8')
net('+5V', 'U9.16', 'U10.16', 'C31.1', 'C32.1')
net('GND', 'C31.2', 'C32.2')

# glue: PLA U19, 74LS175 latch U21, 74LS139 U20, 74LS27 U22
pla_a = {15: 7, 14: 26, 13: 23, 12: 2, 11: 22, 10: 21, 9: 24, 8: 25, 7: 3, 6: 4, 5: 5, 4: 6}
for a, p in pla_a.items():
    net(A[a], 'U19.%d' % p)
net('+5V', 'U19.28', 'C46.1')
net('GND', 'U19.14', 'U19.19', 'C46.2')
net('PHI2', 'U19.17', 'U3.27', 'FB58.1')
net('PLA_F0', 'U19.18')
net('/6551', 'U19.15', 'U3.3')
net('/USERPORT', 'U19.16', 'U5.19')
net('ADDRCLK', 'U19.13', 'U22.1')
net('KERN', 'U19.11', 'U22.5', 'U22.9')
net('F7', 'U19.10', 'U19.9')
net('A0', 'U21.5')
net('A1', 'U21.12')
net('A2', 'U21.13')
net('A3', 'U21.4')
net('LATCH_CLK', 'U22.12', 'U21.9')
net('LATCH_A0', 'U21.7', 'U20.2')
net('LATCH_A1', 'U21.10', 'U20.3')
net('LATCH_NA2', 'U21.14', 'U22.3')
net('LATCH_NA3', 'U21.3', 'U22.11')
net('HI_A0', 'U22.6', 'U20.14')
net('HI_A1', 'U22.8', 'U20.13')
net('GND', 'U22.4', 'U22.10', 'U22.13', 'U22.7', 'U21.8', 'U20.8', 'C49.2', 'C50.2')
net('+5V', 'U22.14', 'U21.16', 'U20.16', 'C49.1', 'C50.1')
net('/C1LOW', 'U20.6', 'CN4.B')
net('/C2LOW', 'U20.7', 'CN4.7')
net('/C1HIGH', 'U20.10', 'CN4.6')
net('/C2HIGH', 'U20.9', 'CN4.8')

# 6551 ACIA U3 and the user port
net('+5V', 'U3.15', 'U3.2', 'C42.1')
net('GND', 'U3.1', 'U3.9', 'C42.2')
net('A0', 'U3.13')
net('A1', 'U3.14')
net('ACIA_X1', 'U3.6', 'Y2.1')
net('ACIA_X2', 'U3.7', 'Y2.2')
net('USR_TXD', 'U3.10', 'CN9.M')
net('USR_RXD', 'U3.12', 'CN9.C')
net('USR_RXC', 'U3.5', 'CN9.8')
net('RTS_T', 'U3.8', 'U6.1')
net('USR_RTS', 'U6.2', 'CN9.D')
net('DTR_T', 'U3.11', 'U6.11')
net('USR_DTR', 'U6.10', 'CN9.E')
net('USR_DCD', 'CN9.H', 'U6.9', 'R26.1')
net('DCD_T', 'U6.8', 'U3.16')
net('USR_DSR', 'CN9.L', 'U6.5', 'R27.1')
net('DSR_T', 'U6.6', 'U3.17')
net('+5V', 'R26.2', 'R27.2', 'U6.14', 'C44.1')
net('GND', 'U6.7', 'C44.2', 'U6.3', 'U6.13')
net('USR_CTS', 'CN9.K', 'U5.3')
net('USR_P0', 'U5.2', 'CN9.B')
net('USR_P3', 'U5.5', 'CN9.5')
net('USR_P4', 'U5.6', 'CN9.6')
net('USR_P5', 'U5.7', 'CN9.7')
net('USR_P6', 'U5.8', 'CN9.J')
net('USR_P7', 'U5.9', 'CN9.F')
net('+5V', 'U5.20', 'C43.1')
net('GND', 'U5.10', 'C43.2')
net('USR_GND1', 'FB59.2', 'CN9.A', 'CN9.1')
net('USR_GND2', 'FB60.2', 'CN9.N', 'CN9.12')
net('USR_5V', 'FB61.2', 'CN9.2')
net('USR_RES', 'FB62.2', 'CN9.3')
net('USR_ATN', 'FB63.2', 'CN9.9')
net('9VAC_A', 'CN9.10', 'FB64.2')
net('9VAC_B', 'CN9.11', 'FB65.2')
net('GND', 'FB59.1', 'FB60.1')
net('+5V', 'FB61.1')

# clock: crystal Y1 (the sim drives CLKIN from it), Q1 (oscillator) and Q2 (buffer)
net('Q1B', 'R1.1', 'R2.2', 'C12.1', 'Q1.3')
net('+5V', 'R2.1', 'R3.1', 'R5.1', 'L5.2', 'C16.1')
net('GND', 'R1.2', 'R4.2', 'C13.2', 'R6.2', 'Q2.1', 'C15.2', 'C16.2')
net('Y1B', 'Y1.2', 'C10.1', 'C11.1')
net('GND', 'C10.2', 'C11.2')
net('Q1E', 'Q1.1', 'C13.1', 'C12.2', 'R4.1')
net('Q1C', 'Q1.2', 'R3.2', 'C14.1')
net('Q2B', 'Q2.3', 'R5.2', 'R6.1', 'C14.2')
net('Q2VCC', 'R7.2', 'L5.1', 'C15.1')

# video/audio: modulator M1 and connector CN7
net('+5V', 'L4.1')
net('M1_VCC', 'L4.2', 'M1.3', 'C21.1')
net('GND', 'C21.2', 'M1.CASE')
net('LUM', 'FB20.2', 'M1.4')
net('COLOR', 'FB21.2', 'M1.5')
net('SND_B', 'FB19.2', 'R10.1')
net('SND_C', 'R10.2', 'C18.1')
net('Q3IN', 'C18.2', 'C17.2', 'R11.1', 'R12.2', 'C19.1', 'R13.1')
net('+5V', 'R12.1', 'Q3.2')
net('GND', 'R11.2', 'C19.2', 'R8.2', 'R15.2', 'R16.2')
net('Q3B', 'R13.2', 'Q3.3')
net('AUDIO', 'Q3.1', 'M1.2', 'R14.1')
net('AUD_N', 'R14.2', 'R15.1', 'C20.1')
net('AUD_OUT', 'C20.2', 'R16.1', 'FB6.1')
net('EXT_AUDIO', 'R8.1', 'R9.1', 'FB8.1', 'CN4.23')
net('EXT_C', 'R9.2', 'C17.1')
net('LUM_OUT', 'M1.8', 'FB3.1')
net('COLOR_OUT', 'M1.6', 'FB4.1')
net('COMP_OUT', 'M1.7', 'FB5.1')
net('GND', 'FB7.1')
net('CN7_1', 'FB3.2', 'CN7.1')
net('CN7_6', 'FB4.2', 'CN7.6')
net('CN7_4', 'FB5.2', 'CN7.4')
net('CN7_3', 'FB6.2', 'CN7.3')
net('CN7_2', 'FB7.2', 'CN7.2')
net('CN7_5', 'FB8.2', 'CN7.5')

# expansion connector CN4
for i, p in enumerate(['Y', 'X', 'W', 'V', 'U', 'T', 'S', 'R', 'P', 'N', 'M', 'L', 'K', 'J', 'H', 'F']):
    net(A[i], 'CN4.%s' % p)
for i in range(8):
    net(D[i], 'CN4.%d' % (21 - i))
net('X_RAS', 'FB15.2', 'CN4.D')
net('X_PHI0', 'FB16.2', 'CN4.E')
net('X_CAS', 'FB17.2', 'CN4.11')
net('X_PHI2', 'FB58.2', 'CN4.24')
net('+5V', 'CN4.2', 'CN4.3')
net('GND', 'CN4.1', 'CN4.A', 'CN4.CC', 'CN4.25')

# power: the +5V arrives from outside (CN1 pin 1) through the filter L1 and the switch; the 9VAC through the
# line filter L2, the switch, the fuse F1 and the bridge CR1
net('5V_RAW', 'CN1.1', 'C100.1', 'L1.1')
net('GND', 'CN1.2', 'C100.2')
net('5V_F', 'L1.2', 'SW1.1', 'C2.1')
net('GND', 'C2.2')
net('5V_SW', 'SW1.2', 'C1.1')
net('GND', 'C1.2')
net('9V_IN', 'CN1.3', 'C3.1', 'L2A.1')
net('9V_RET', 'CN1.4', 'C3.2', 'L2B.1')
net('9V_L', 'L2A.2', 'C5.2', 'SW1B.1')
net('9V_SW', 'SW1B.2', 'F1.1', 'C4.1', 'FB64.1')
net('9V_FUSED', 'F1.2', 'CR1.1', 'C6.1')
net('9V_B', 'L2B.2', 'C4.2', 'C5.1', 'C6.2', 'CR1.3', 'FB65.1')
net('UNREG', 'CR1.2', 'C7.1', 'C8.1', 'C9.1')
net('GND', 'CR1.4', 'C7.2', 'C8.2', 'C9.2')

# ----------------------------------------------------------------------------------------------------------------
# Placement (millimetres). The left half of the board is the left half of the layout drawing (RAM, the expansion
# port on top, the user port, the power), the right half the right one (ROMs, CPU, TED, the keyboard on top, the
# serial, cassette and joystick connectors along the bottom).
# ----------------------------------------------------------------------------------------------------------------
# DIP centres: 'h' pins in two horizontal rows (pin 1 bottom left), 'v' two columns (pin 1 top left)
ICS = {
    'U18': (40, 24, 'h'), 'U17': (40, 36, 'h'), 'U16': (40, 48, 'h'), 'U15': (40, 60, 'h'), 'U14': (40, 72, 'h'),
    'U13': (40, 84, 'h'), 'U12': (40, 96, 'h'), 'U11': (40, 108, 'h'), 'U9': (35, 122, 'h'), 'U10': (35, 134, 'h'),
    'U26': (95, 135, 'h'), 'U19': (115, 60, 'h'), 'U3': (115, 30, 'h'), 'U5': (115, 85, 'h'), 'U20': (82, 85, 'h'),
    'U21': (82, 100, 'h'), 'U22': (82, 114, 'h'), 'U6': (148, 70, 'h'),
    'U25': (205, 16, 'h'), 'U24': (205, 36, 'h'), 'U23': (205, 56, 'h'), 'U27': (265, 42, 'h'), 'U2': (240, 100, 'h'),
    'U1': (325, 62, 'v'), 'U8': (190, 85, 'h'), 'U7': (203, 116, 'h'), 'U4': (300, 118, 'h'),
}
FIXED = {
    'CN4': (62, 8), 'CN9': (168, 40), 'CN1': (155, 140), 'CN7': (130, 142), 'M1': (140, 118), 'CN8': (262, 12),
    'CN2': (230, 138), 'CN3': (252, 138), 'CN5': (275, 138), 'CN6': (298, 138),
    'Y1': (318, 110), 'Y2': (150, 14), 'Q1': (305, 128), 'Q2': (318, 128), 'Q3': (330, 128), 'Q4': (170, 118),
    'SW1': (165, 100), 'SW2': (175, 85), 'F1': (150, 100),
}


def dip(ref, cx, cy, d):
    value, fp, names = parts[ref]
    n = len(names)
    w = float(fp.split('_W')[1][:-2])
    half = n // 2
    pads = []
    for pin in range(1, n + 1):
        if pin <= half:
            x, y = -w / 2, (pin - 1 - (half - 1) / 2) * 2.54
        else:
            x, y = w / 2, ((half - 1) / 2 - (pin - half - 1)) * 2.54
        if d == 'h':
            x, y = y, -x
        pads.append([str(pin), cx + x, cy + y, 1.6, 1.6])
    return pads


def row_pads(ref, cx, cy, pitch=2.54, cols=1):
    value, fp, names = parts[ref]
    n = len(names)
    per = (n + cols - 1) // cols
    pads = []
    for i, nm in enumerate(names):
        c, r = divmod(i, per)
        pads.append([nm, cx + (c - (cols - 1) / 2) * pitch + 0.0, cy + (r - (per - 1) / 2) * pitch, 1.7, 1.7])
    return pads


def horiz_pads(ref, cx, cy, pitch=2.54):
    value, fp, names = parts[ref]
    n = len(names)
    return [[nm, cx + (i - (n - 1) / 2) * pitch, cy, 1.7, 1.7] for i, nm in enumerate(names)]


def two_pin(ref, cx, cy):
    value, fp, names = parts[ref]
    span = 10.16 if ref.startswith('R') else 5.08
    if len(names) == 1:
        return [[names[0], cx, cy, 1.8, 1.8]]
    if len(names) == 3:
        return [[nm, cx + (i - 1) * 2.54, cy, 1.8, 1.8] for i, nm in enumerate(names)]
    return [[names[0], cx - span / 2, cy, 1.8, 1.8], [names[1], cx + span / 2, cy, 1.8, 1.8]]


def bbox(pads):
    return (min(p[1] - p[3] / 2 for p in pads), min(p[2] - p[4] / 2 for p in pads),
            max(p[1] + p[3] / 2 for p in pads), max(p[2] + p[4] / 2 for p in pads))


def overlaps(r, rects, margin):
    return any(r[0] < q[2] + margin and q[0] < r[2] + margin and r[1] < q[3] + margin and q[1] < r[3] + margin
               for q in rects)


def make_pads(ref, cx, cy):
    value, fp, names = parts[ref]
    if ref in ICS and ref in parts and fp.startswith('DIP-'):
        return dip(ref, cx, cy, ICS[ref][2])
    if ref == 'CN4':
        return [[nm, cx + (i % 25 - 12) * 2.54, cy + (0 if i < 25 else 5.08), 1.7, 1.7] for i, nm in enumerate(names)]
    if ref == 'CN8':
        return horiz_pads(ref, cx, cy)
    if ref.startswith('CN') or ref == 'M1':
        cols = 2 if len(names) > 6 else 1
        return row_pads(ref, cx, cy, 2.54, cols)
    if ref.startswith('RP') or ref == 'CR1' or ref.startswith('Q'):
        return horiz_pads(ref, cx, cy)
    return two_pin(ref, cx, cy)


def main():
    # pads and the net of each
    padnet = {}
    for n, ps in nets.items():
        for p in ps:
            if p in padnet and padnet[p] != n:
                # a pad on two nets: the later net wins, but report it
                sys.stderr.write('pad %s on %s and %s\n' % (p, padnet[p], n))
            padnet[p] = n
    for ref, (v, fp, names) in parts.items():
        pass
    placed = {}
    rects = []
    comps = {}
    for ref in parts:
        if ref in ICS:
            cx, cy = ICS[ref][0], ICS[ref][1]
            comps[ref] = make_pads(ref, cx, cy)
            placed[ref] = (cx, cy)
        elif ref in FIXED:
            cx, cy = FIXED[ref][0], FIXED[ref][1]
            comps[ref] = make_pads(ref, cx, cy)
            placed[ref] = (cx, cy)
        else:
            continue
        x0, y0, x1, y1 = bbox(comps[ref])
        rects.append((x0 - 1.0, y0 - 1.0, x1 + 1.0, y1 + 1.0))
    # which chips a net touches
    net_ics = {}
    for n, ps in nets.items():
        if n in ('GND', '+5V'):
            continue
        for p in ps:
            r = p.split('.')[0]
            if r in ICS or r in ('CN1', 'CN2', 'CN3', 'CN4', 'CN5', 'CN6', 'CN7', 'CN8', 'CN9', 'M1'):
                net_ics.setdefault(n, []).append(r)
    for ref in parts:
        if ref in placed:
            continue
        count = {}
        for pin in parts[ref][2]:
            n = padnet.get('%s.%s' % (ref, pin))
            for i in net_ics.get(n, []):
                count[i] = count.get(i, 0) + 1
        if count:
            best = max(count.values())
            anchor = [i for i in count if count[i] == best][0]
        else:
            anchor = 'U2'
        ax, ay = placed[anchor]
        probe = make_pads(ref, 0, 0)
        x0, y0, x1, y1 = bbox(probe)
        w, h = x1 - x0, y1 - y0
        cands = sorted(((ax + dx * 2.5, ay + dy * 2.5) for dx in range(-40, 41) for dy in range(-30, 31)),
                       key=lambda q: math.hypot(q[0] - ax, q[1] - ay))
        spot = None
        for qx, qy in cands:
            r = (qx - w / 2, qy - h / 2, qx + w / 2, qy + h / 2)
            if r[0] < OUTLINE[0] + 2 or r[1] < OUTLINE[1] + 2 or r[2] > OUTLINE[2] - 2 or r[3] > OUTLINE[3] - 2:
                continue
            if not overlaps(r, rects, 0.8):
                spot = (qx, qy)
                break
        if spot is None:
            sys.exit('no place for ' + ref)
        comps[ref] = make_pads(ref, *spot)
        placed[ref] = spot
        rects.append((spot[0] - w / 2, spot[1] - h / 2, spot[0] + w / 2, spot[1] + h / 2))

    names = []
    for ref in parts:
        for pin in parts[ref][2]:
            n = padnet.get('%s.%s' % (ref, pin))
            if n and n not in names:
                names.append(n)
    # rails first
    for r in ('+5V', 'GND'):
        if r in names:
            names.remove(r)
    names = ['GND', '+5V'] + names
    index = {n: i for i, n in enumerate(names)}
    out = sys.stdout
    out.write('/* plus4_board_data.c - GENERATED by tools/plus4_board.py, do not edit.\n')
    out.write(' * Commodore Plus/4 (schematic 310164 rev C, 4 sheets). Netlist typed from the schematic;\n')
    out.write(' * the chips and connectors are placed after the layout drawing of the service manual, the passives only near their chips. */\n')
    out.write('#include "board_data.h"\n\nstatic const char *const nets[] = {\n')
    for n in names:
        out.write('    "%s",\n' % n.replace('\\', '\\\\').replace('"', '\\"'))
    out.write('};\n\nstatic const bd_pad_t pads[] = {\n')
    total = 0
    order = list(parts)
    for ref in order:
        for p, pin in zip(comps[ref], parts[ref][2]):
            n = padnet.get('%s.%s' % (ref, pin))
            out.write('    { "%s", %.3ff, %.3ff, %.3ff, %.3ff, %d },\n' % (
                p[0], p[1], p[2], p[3], p[4], index[n] if n else -1))
    out.write('};\n\nstatic const bd_comp_t comps[] = {\n')
    first = 0
    for ref in order:
        v, fp, pn = parts[ref]
        out.write('    { "%s", "%s", "%s", %d, %d },\n' % (ref, v, fp, first, len(pn)))
        first += len(pn)
    out.write('};\n\nconst board_data_t bd_plus4 = {\n    { %.3ff, %.3ff, %.3ff, %.3ff },\n' % OUTLINE)
    out.write('    nets, %d, pads, %d, comps, %d,\n};\n' % (len(names), first, len(order)))


if __name__ == '__main__':
    main()
