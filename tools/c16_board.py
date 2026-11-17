#!/usr/bin/env python3
"""c16_board.py - the Commodore 16 board (ASSY 250443, PCB 251789, schematic 251788 sheets 1-3)

usage: python3 tools/c16_board.py > src/c16_board_data.c

The netlist is typed in below from the three sheets of the schematic (no KiCad project exists). The chips and
the connectors are put where the photo of an ASSY 250443 has them (3.54 pixels to the millimetre, the photo is
1200 x 529); the passives are put in free places next to the chip they are connected to, not where the board
has them."""
import math
import sys

PX = 1 / 3.54          # millimetres per photo pixel
OUTLINE = (0.0, 0.0, 340.0, 150.0)

# ----------------------------------------------------------------------------------------------------------------
# Parts: ref -> (value, footprint, pins)  (pins: a count, or a list of pad names)
# ----------------------------------------------------------------------------------------------------------------
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
part('U2', '7501', dipfp(40, 15.24), 40)
part('U3', '318006-01', dipfp(28, 15.24), 28)
part('U4', '318005-04', dipfp(28, 15.24), 28)
part('U5', '4416', dipfp(18, 7.62), 18)
part('U6', '4416', dipfp(18, 7.62), 18)
part('U7', '74LS257', dipfp(16, 7.62), 16)
part('U8', '74LS257', dipfp(16, 7.62), 16)
part('U9', '7406', dipfp(14, 7.62), 14)
part('U10', '555', dipfp(8, 7.62), 8)
part('U11', '74LS125', dipfp(14, 7.62), 14)
part('U12', '74LS02', dipfp(14, 7.62), 14)
part('U13', '6529B', dipfp(20, 7.62), 20)
part('U14', '74LS139', dipfp(16, 7.62), 16)
part('U15', '74LS175', dipfp(16, 7.62), 16)
part('U16', '7700-010', dipfp(28, 15.24), 28)

# Connectors
EXP = ['1', '2', '3', '4', '5', '6', '7', '8', '9', '10', '11', '12', '13', '14', '15', '16', '17', '18', '19', '20',
       '21', '22', '23', '24', '25', 'A', 'B', 'C', 'D', 'E', 'F', 'H', 'J', 'K', 'L', 'M', 'N', 'P', 'R', 'S', 'T',
       'U', 'V', 'W', 'X', 'Y', 'Z', 'AA', 'BB', 'CC']
part('CN1', 'Expansion', 'C16_EXPANSION_2x25', EXP)
part('CN2', 'Keyboard', 'PinHeader_1x20_P2.54mm_Vertical', 20)
part('CN3', 'Cassette', 'DIN-7', 7)
part('CN4', 'Joystick 1', 'DIN-8', 8)
part('CN5', 'Joystick 2', 'DIN-8', 8)
part('CN6', 'Audio/Video', 'DIN-8', 8)
part('CN7', 'Serial bus', 'DIN-6', 6)
part('CN8', 'Power', 'Jack_DC', 2)
part('CN9', 'LED', 'PinHeader_1x03_P2.54mm_Vertical', 3)
part('M1', 'RF modulator', 'RF_MODULATOR', ['2', '3', '4', '5', '6', '7', '8', 'CASE'])

# Resistors, capacitors, others
R = {'R1': '10K', 'R2': '4.7K', 'R3': '470', 'R4': '220', 'R5': '18K', 'R6': '1.5K', 'R7': '470', 'R8': '1K',
     'R9': '12K', 'R10': '20 5W', 'R11': '100K', 'R12': '3.3K', 'R13': '470', 'R14': '470', 'R19': '3.3K',
     'R20': '47K', 'R21': '100K', 'R23': '10K', 'R24': '1K', 'R25': '100K', 'R26': '240', 'R27': '240',
     'R28': '1K', 'R29': '100', 'R102': '330'}
for r, v in R.items():
    part(r, v, 'R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal', 2)
C = {'C1': '.1uF', 'C2': '47uF', 'C3': '10uF', 'C4': '10uF', 'C5': '.1uF', 'C6': '.1uF', 'C7': '.22uF',
     'C8': '.22uF', 'C10': '.1uF', 'C11': '.1uF', 'C12': '22pF', 'C13': '220pF', 'C14': '150pF', 'C15': '.01uF',
     'C16': '10uF', 'C17': '1uF', 'C18': '10uF', 'C19': '10uF', 'C20': '.01uF', 'C21': '.1uF', 'C22': '.1uF',
     'C23': '470pF', 'C24': '470pF', 'C25': '470pF', 'C26': '.1uF', 'C28': '.1uF', 'C30': '.1uF', 'C31': '10uF',
     'C32': '.1uF', 'C33': '.1uF', 'C34': '.1uF', 'C200': '.22uF', 'CT1': '40pF'}
for c, v in C.items():
    part(c, v, 'C_Disc_D4.7mm_W2.5mm_P5.00mm', 2)
part('RP1', '1K', 'RP_ARRAY_SIP6', 6)
part('RP2', '3.3K', 'RP_ARRAY_SIP6', 6)
part('RP5', '3.3K', 'RP_ARRAY_SIP6', 6)
part('RP3', '68 isolated', 'RP_ISOLATED_SIP8', 8)
part('RP4', '68 isolated', 'RP_ISOLATED_SIP8', 8)
for i in range(1, 11):
    part('D%d' % i, '1N914B', 'D_DO-35_P7.62mm_Horizontal', 2)
part('D11', '1N4001', 'D_DO-41_P10.16mm_Horizontal', 2)
part('D12', '6.8V', 'D_DO-35_P7.62mm_Horizontal', 2)
part('Q1', '2SC1815', 'Q_NPN_ECB', 3)
part('Q2', '2SC1815', 'Q_NPN_ECB', 3)
part('Q3', '2SC1815', 'Q_NPN_ECB', 3)
part('Q5', '2SD880', 'Q_NPN_ECB', 3)
part('VR1', '7805', 'TO-220-3_Vertical', 3)
part('Y1', '14.31818 MHz', 'Crystal_HC18-U_Vertical', 2)
part('F1', '1.5A', 'Fuseholder_Cylinder-5x20mm', 2)
part('SW1', 'POWER', 'SW_Slide', 2)
part('SW2', 'RESET', 'SW_Push', 2)
part('L1', '1.2uH', 'L_Axial_L7.0mm_D3.3mm_P5.08mm_Vertical', 2)
part('L3A', 'LINE FILTER', 'L_Axial_L7.0mm_D3.3mm_P5.08mm_Vertical', 2)
part('L3B', 'LINE FILTER', 'L_Axial_L7.0mm_D3.3mm_P5.08mm_Vertical', 2)
part('TP', 'CLK TP', 'TestPoint_Pad_D1.0mm', 1)
FB = list(range(1, 6)) + list(range(12, 20)) + list(range(22, 23)) + list(range(24, 30)) + list(range(30, 37)) \
    + list(range(38, 41)) + list(range(41, 57)) + [58]
for n in FB:
    part('FB%d' % n, 'ferrite', 'R_Axial_Link_FB', 2)
for e in ('EM1', 'EM2', 'EM3', 'EM4', 'EM5'):
    part(e, 'EMI', 'EMI_T-FILTER', 3)

# ----------------------------------------------------------------------------------------------------------------
# Nets
# ----------------------------------------------------------------------------------------------------------------
A = ['A%d' % i for i in range(16)]
D = ['D%d' % i for i in range(8)]

# TED (U1): A0-A2 pins 3,2,1; A3-A15 pins 48..36
ted_a = {0: 3, 1: 2, 2: 1}
for i in range(3, 16):
    ted_a[i] = 51 - i
for i in range(16):
    net(A[i], 'U1.%d' % ted_a[i])
for i in range(8):
    net(D[i], 'U1.%d' % (25 + i), 'U2.%d' % (38 - i), 'U13.%d' % (18 - i))
    net('K%d' % i, 'U1.%d' % (15 + i))
# the CPU's address pins: A0-A13 on 6-19, A14 on 21, A15 on 22
for i in range(16):
    net(A[i], 'U2.%d' % ([6 + k for k in range(14)] + [21, 22])[i])
net('+5V', 'U1.4', 'U2.5')
net('GND', 'U1.24', 'U2.20')
net('R/W', 'U1.7', 'U2.39', 'U13.1', 'U12.3', 'U5.4', 'U6.4', 'CN1.5')
net('IRQ', 'U1.8', 'U2.3', 'R19.1', 'CN1.4')
net('MUX', 'U1.9', 'U2.23', 'U7.1', 'U8.1', 'U16.27', 'CN1.12')
net('/RAS', 'U1.10', 'U5.5', 'U6.5', 'U16.20', 'FB38.1')
net('/CAS', 'U1.11', 'U5.16', 'U6.16', 'FB40.1')
net('PHI0', 'U1.12', 'U2.1', 'U16.8', 'FB39.1')
net('COLOR', 'U1.13', 'M1.5')
net('CLKIN', 'U1.14', 'R7.1', 'TP.1', 'Y1.1')
net('LUM', 'U1.23', 'M1.4')
net('SND', 'U1.33', 'R8.1')
net('BA', 'U1.34', 'U2.2', 'CN1.13')
net('AEC', 'U1.35', 'U2.4', 'CN1.22')
net('/CS0', 'U1.5', 'U14.1', 'CN1.10')
net('/CS1', 'U1.6', 'U14.15', 'CN1.9')

# power on reset: the 555 and the 7406 U9 (gates 1>2 reset, 3>4 serial reset, 5>6 motor, 9>8 ATN, 11>10 CLK, 13>12 DATA)
net('PWR', 'U10.3', 'U9.1', 'U9.3')
net('/RESET', 'U9.2', 'U2.40', 'RP1.2', 'U15.1', 'CN1.C')
net('SERRST', 'U9.4', 'RP1.4', 'FB2.1')
net('555_THR', 'U10.6', 'U10.7', 'R20.2', 'C16.1')
net('555_TRG', 'U10.2', 'R21.2', 'C17.1', 'SW2.1')
net('+5V', 'U10.4', 'U10.8', 'R20.1', 'R21.1', 'RP1.1')
net('GND', 'U10.1', 'C16.2', 'C17.2', 'SW2.2')
net('GND', 'U9.7')
net('+5V', 'U9.14', 'C34.1')
net('GND', 'C34.2')

# serial bus, cassette port (CPU port bits)
net('SER_ATN_OUT', 'U2.28', 'U9.9')
net('SER_CLK_OUT', 'U2.29', 'U9.11')
net('SER_DATA_OUT', 'U2.30', 'U9.13')
net('CAS_MTR', 'U2.27', 'U9.5')
net('CAS_RD', 'U2.26', 'FB16.1', 'C23.1')
net('SER_ATN', 'U9.8', 'RP1.3', 'FB5.1')
net('SER_CLK', 'U9.10', 'RP1.5', 'FB4.1', 'U2.25', 'FB17.1', 'C24.1')      # P6; the cassette write is this line
net('SER_DATA', 'U9.12', 'RP1.6', 'FB3.1', 'U2.24')                         # P7
net('CN7_3', 'FB5.2', 'CN7.3')
net('CN7_4', 'FB4.2', 'CN7.4')
net('CN7_5', 'FB3.2', 'CN7.5')
net('CN7_6', 'FB2.2', 'CN7.6')
net('CN7_1', 'CN7.1', 'R102.1')
net('+5V', 'R102.2')
net('CN7_2', 'CN7.2', 'FB1.1')
net('GND', 'FB1.2')

# cassette connector CN3
net('MTR_B', 'U9.6', 'Q5.3', 'R13.2', 'D12.1')
net('MTR', 'Q5.1', 'FB19.1')
net('CST_SENSE', 'U11.9', 'R12.1', 'R29.1')
net('+5V', 'R12.2', 'R19.2')
net('/CASSETTE', 'U11.10', 'U16.16')
net('SENSE_C', 'R29.2', 'FB18.1', 'C25.1')
net('GND', 'C23.2', 'C24.2', 'C25.2', 'U11.1', 'U11.4', 'U11.7')
net('+5V', 'U11.14', 'FB14.1', 'C26.1')
net('GND', 'C26.2', 'FB15.1')
net('CN3_1', 'FB15.2', 'CN3.1')
net('CN3_2', 'FB14.2', 'CN3.2')
net('CN3_3', 'FB19.2', 'CN3.3')
net('CN3_4', 'FB16.2', 'CN3.4')
net('CN3_5', 'FB17.2', 'CN3.5')
net('CN3_6', 'FB18.2', 'CN3.6')
net('GND', 'CN3.7')
net('D2', 'U11.8', 'U11.2')
net('D1', 'U11.5')

# keyboard: 6529 (U13) ports P0-P7 are the rows, the TED's K0-K7 the columns
row_pin = [19, 8, 12, 11, 13, 1, 16, 6]
col_pin = [18, 15, 14, 17, 7, 9, 10, 3]
net('/KEYPORT', 'U13.19', 'U16.12')
net('+5V', 'U13.20', 'C33.1')
net('GND', 'U13.10', 'C33.2')
for i in range(8):
    net('ROW%d' % i, 'U13.%d' % (2 + i), 'FB%d.1' % (41 + i))
    net('CN2_%d' % row_pin[i], 'FB%d.2' % (41 + i), 'CN2.%d' % row_pin[i])
    net('K%d' % i, 'FB%d.1' % (49 + i))
    net('CN2_%d' % col_pin[i], 'FB%d.2' % (49 + i), 'CN2.%d' % col_pin[i])
net('GND', 'CN2.4')

# joysticks: pins 1 2 3 4 6 (up down left right fire) through FB24-28 / FB31-35 to RP2 / RP5 and the diodes
for jn, (cn, fbs, rp, dio, sel, ff, em) in enumerate([
        ('CN4', [24, 25, 26, 27, 28], 'RP2', [1, 2, 3, 4, 5], 'U11.3', 22, 'EM1'),
        ('CN5', [31, 32, 33, 34, 35], 'RP5', [6, 7, 8, 9, 10], 'U11.6', 30, 'EM2')]):
    for k, jp in enumerate([1, 2, 3, 4, 6]):
        n = 'J%dP%d' % (jn + 1, jp)
        net(n + 'C', 'FB%d.1' % fbs[k], 'D%d.1' % dio[k], '%s.%d' % (rp, [2, 6, 3, 5, 4][k]))
        net(n + 'J', 'FB%d.2' % fbs[k], '%s.%d' % (cn, jp))
    net('+5V', '%s.1' % rp, 'FB%d.1' % ff)
    net('J%dP5' % (jn + 1), 'FB%d.2' % ff, '%s.5' % cn)
    net('J%dSEL' % (jn + 1), sel, '%s.3' % em)
    net('J%dSEL_C' % (jn + 1), '%s.1' % em, '%s.8' % cn)
    net('GND', '%s.2' % em)
net('GND', 'FB29.1', 'FB36.1')
net('J1P7', 'FB29.2', 'CN4.7')
net('J2P7', 'FB36.2', 'CN5.7')
# the diodes: anode (pin 2) on the K line, cathode (pin 1) on the joystick
for k, d in enumerate([1, 2, 3, 4, 5]):
    net('K%d' % [0, 1, 2, 3, 6][k], 'D%d.2' % d)
for k, d in enumerate([6, 7, 8, 9, 10]):
    net('K%d' % [0, 1, 2, 3, 7][k], 'D%d.2' % d)
net('D2', 'U11.2')
net('D1', 'U11.5')

# memory: ROMs, DRAMs and the address multiplexers
rom_a = [10, 9, 8, 7, 6, 5, 4, 3, 25, 24, 21, 23, 2, 26]
rom_d = [11, 12, 13, 15, 16, 17, 18, 19]
for r in ('U3', 'U4'):
    for i in range(14):
        net(A[i], '%s.%d' % (r, rom_a[i]))
    for i in range(8):
        net(D[i], '%s.%d' % (r, rom_d[i]))
    net('+5V', '%s.28' % r, '%s.20' % r, '%s.27' % r)
    net('GND', '%s.14' % r)
net('/BASIC_CS', 'U3.22', 'U14.4')
net('/KERNAL_CS', 'U4.22', 'U14.12')
net('+5V', 'C5.1', 'C6.1')
net('GND', 'C5.2', 'C6.2')
ram_dq = [2, 3, 15, 17]
ram_a = [14, 13, 12, 11, 8, 7, 6, 10]
for r, base in (('U5', 0), ('U6', 4)):
    for i in range(4):
        net(D[base + i], '%s.%d' % (r, ram_dq[i]))
    net('+5V', '%s.9' % r)
    net('GND', '%s.18' % r, '%s.1' % r)
for i in range(4):
    net('MA%d' % i, 'RP3.%d' % (2 * i + 2), 'U5.%d' % ram_a[i], 'U6.%d' % ram_a[i])
    net('MA%d_M' % i, 'RP3.%d' % (2 * i + 1), 'U7.%d' % [4, 7, 9, 12][i])
    net('MA%d' % (i + 4), 'RP4.%d' % (2 * i + 2), 'U5.%d' % ram_a[i + 4], 'U6.%d' % ram_a[i + 4])
    net('MA%d_M' % (i + 4), 'RP4.%d' % (2 * i + 1), 'U8.%d' % [4, 7, 9, 12][i])
# multiplexer inputs: A (selected while MUX is low: the column) and B (MUX high: the row)
for i, (pa, pb) in enumerate([(2, 3), (5, 6), (11, 10), (14, 13)]):
    net(A[i], 'U7.%d' % pb)
    net(A[i + 4], 'U8.%d' % pb)
net('+5V', 'U7.2', 'U8.14')
net('A8', 'U7.5')
net('A9', 'U7.11')
net('A10', 'U7.14')
net('A11', 'U8.2')
net('A12', 'U8.5')
net('A13', 'U8.11')
net('GND', 'U7.15', 'U7.8', 'U8.15', 'U8.8')
net('+5V', 'U7.16', 'U8.16', 'C10.1', 'C7.1', 'C8.1')
net('GND', 'C10.2', 'C7.2', 'C8.2')

# glue: PLA, 74LS175 latch, 74LS139, 74LS02
pla_a = {15: 7, 14: 26, 13: 23, 12: 2, 11: 22, 10: 21, 9: 24, 8: 25, 7: 3, 6: 4, 5: 5, 4: 6}
for a, p in pla_a.items():
    net(A[a], 'U16.%d' % p)
net('+5V', 'U16.28', 'C30.1')
net('GND', 'U16.14', 'U16.19', 'C30.2')
net('PHI2', 'U16.17', 'CN1.24')
net('PLA_F0', 'U16.18')
net('/6551', 'U16.15')
net('ADDRCLK', 'U16.13', 'U12.2')
net('KERN', 'U16.11', 'U12.6', 'U12.9')
net('F7', 'U16.10', 'U16.9')
net('A0', 'U15.4')
net('A1', 'U15.13')
net('A2', 'U15.5')
net('A3', 'U15.12')
net('LATCH_CLK', 'U12.1', 'U15.9')
net('LATCH_A0', 'U15.2', 'U14.2')
net('LATCH_A1', 'U15.15', 'U14.3')
net('LATCH_NA2', 'U15.6', 'U12.5')
net('LATCH_NA3', 'U15.11', 'U12.8')
net('HI_A0', 'U12.4', 'U14.14')
net('HI_A1', 'U12.10', 'U14.13')
net('+5V', 'U15.16', 'U12.14', 'U14.16', 'C28.1')
net('GND', 'U15.8', 'U12.7', 'U14.8', 'C28.2')
net('/C1LOW', 'U14.6', 'CN1.B')
net('/C2LOW', 'U14.7', 'CN1.7')
net('/C1HIGH', 'U14.10', 'CN1.6')
net('/C2HIGH', 'U14.9', 'CN1.8')

# clock: crystal, Q1 (oscillator) and Q2 (buffer)
net('Q1B', 'R1.2', 'R2.1', 'C13.1', 'Q1.3')
net('GND', 'R2.2', 'R4.2', 'C14.2', 'C12.2')
net('+5V', 'R1.1', 'R3.1', 'R5.1', 'R7.2', 'C32.1')
net('GND', 'C32.2', 'Q2.1')
net('Y1B', 'Y1.2', 'CT1.1', 'C12.1')
net('GND', 'CT1.2')
net('Q1E', 'Q1.1', 'C13.2', 'C14.1', 'R4.1')
net('Q1C', 'Q1.2', 'R3.2', 'C15.1')
net('Q2B', 'C15.2', 'R5.2', 'R6.1', 'Q2.3')
net('GND', 'R6.2')
net('CLKIN', 'Q2.2')

# video/audio: modulator and connector CN6
net('+5V', 'C11.1', 'L1.1', 'CN6.8')
net('GND', 'C11.2', 'M1.CASE')
net('M1_VCC', 'L1.2', 'M1.3')
net('SND_Y', 'R8.2', 'C19.1')
net('Q3IN', 'C19.2', 'R9.1', 'R23.2', 'R24.1', 'C20.1', 'C18.2')
net('GND', 'R9.2', 'C20.2', 'R25.2', 'R27.2', 'R11.2', 'Q2.1')
net('+5V', 'R23.1', 'Q3.2')
net('Q3B', 'R24.2', 'Q3.3')
net('AUDIO', 'Q3.1', 'M1.2', 'R26.1')
net('AUD_N', 'R26.2', 'R27.1', 'C31.1')
net('AUD_OUT', 'C31.2', 'R11.1', 'FB12.1')
net('EXT_AUDIO', 'R25.1', 'R28.1', 'CN1.23', 'FB58.1')
net('Q3X', 'R28.2', 'C18.1')
net('LUM_OUT', 'M1.8', 'EM3.1')
net('COLOR_OUT', 'M1.6', 'EM4.1')
net('COMP_OUT', 'M1.7', 'EM5.1')
net('GND', 'EM3.2', 'EM4.2', 'EM5.2')
net('CN6_1', 'EM3.3', 'CN6.1')
net('CN6_6', 'EM4.3', 'CN6.6')
net('CN6_4', 'EM5.3', 'CN6.4')
net('CN6_3', 'FB12.2', 'CN6.3')
net('CN6_5', 'FB58.2', 'CN6.5')
net('GND', 'FB13.1')
net('CN6_2', 'FB13.2', 'CN6.2')
net('CN6_7', 'CN6.7')

# expansion connector CN1
for i, p in enumerate(['Y', 'X', 'W', 'V', 'U', 'T', 'S', 'R', 'P', 'N', 'M', 'L', 'K', 'J', 'H', 'F']):
    net(A[i], 'CN1.%s' % p)
for i in range(8):
    net(D[i], 'CN1.%d' % (21 - i))
net('X_RAS', 'FB38.2', 'CN1.D')
net('X_PHI0', 'FB39.2', 'CN1.E')
net('X_CAS', 'FB40.2', 'CN1.11')
net('+5V', 'CN1.2', 'CN1.3')
net('GND', 'CN1.1', 'CN1.A', 'CN1.CC', 'CN1.25')

# power
net('9V_IN', 'CN8.2', 'C200.1', 'L3A.1')
net('9V_RET', 'CN8.1', 'C200.2', 'L3B.1')
net('GND', 'L3B.2')
net('9V_L', 'L3A.2', 'SW1.1')
net('9V_SW', 'SW1.2', 'F1.1')
net('VIN', 'F1.2', 'VR1.1', 'C1.1', 'R10.1', 'D11.1', 'Q5.2', 'R13.1')
net('+5V', 'VR1.3', 'C2.1', 'R10.2', 'D11.2')
net('GND', 'VR1.2', 'C1.2', 'C2.2', 'D12.2')
net('+5V', 'C3.1', 'C4.1', 'C21.1', 'C22.1', 'R14.1')
net('GND', 'C3.2', 'C4.2', 'C21.2', 'C22.2', 'CN9.1', 'CN9.2')
net('LED', 'R14.2', 'CN9.3')

# the CPU's decoupling
net('+5V', 'C26.1')   # (already there)

# ----------------------------------------------------------------------------------------------------------------
# Placement (photo pixels -> mm)
# ----------------------------------------------------------------------------------------------------------------
# DIP centres in photo pixels, 'h' pins in two horizontal rows (pin 1 bottom left), 'v' two columns
ICS = {
    'U1': (597, 372, 'h'), 'U2': (578, 267, 'h'), 'U16': (361, 232, 'h'), 'U4': (365, 333, 'h'),
    'U3': (365, 430, 'h'), 'U5': (222, 363, 'h'), 'U6': (219, 441, 'h'), 'U7': (112, 363, 'h'),
    'U8': (220, 276, 'h'), 'U9': (247, 147, 'h'), 'U10': (176, 147, 'h'), 'U11': (109, 427, 'h'),
    'U12': (120, 276, 'h'), 'U13': (792, 378, 'h'), 'U14': (220, 207, 'h'), 'U15': (120, 207, 'h'),
}
FIXED = {
    'CN3': (155, 52), 'CN7': (360, 48), 'CN6': (462, 48), 'M1': (613, 94), 'CN1': (909, 159), 'CN2': (876, 480),
    'CN4': (1110, 312), 'CN5': (1116, 384), 'CN8': (1116, 138), 'CN9': (1020, 480), 'SW1': (1074, 180),
    'SW2': (1074, 214), 'F1': (954, 213), 'VR1': (888, 312), 'Y1': (492, 447), 'Q1': (560, 440), 'Q2': (610, 440),
    'TP': (650, 480),
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
    if ref == 'CN1':
        return [[nm, cx + (i % 25 - 12) * 2.54, cy + (0 if i < 25 else 5.08), 1.7, 1.7] for i, nm in enumerate(names)]
    if ref == 'CN2':
        return horiz_pads(ref, cx, cy)
    if ref.startswith('CN') or ref == 'M1':
        cols = 2 if len(names) > 6 else 1
        return row_pads(ref, cx, cy, 2.54, cols)
    if ref.startswith('RP') or ref == 'VR1' or ref.startswith('Q'):
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
            cx, cy = ICS[ref][0] * PX, ICS[ref][1] * PX
            comps[ref] = make_pads(ref, cx, cy)
            placed[ref] = (cx, cy)
        elif ref in FIXED:
            cx, cy = FIXED[ref][0] * PX, FIXED[ref][1] * PX
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
            if r in ICS or r in ('CN1', 'CN2', 'CN3', 'CN4', 'CN5', 'CN6', 'CN7', 'M1'):
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
    out.write('/* c16_board_data.c - GENERATED by tools/c16_board.py, do not edit.\n')
    out.write(' * Commodore 16, ASSY 250443 / PCB 251789 (schematic 251788, 3 sheets). Netlist typed from the schematic;\n')
    out.write(' * the chips and connectors are placed as in a photo of the board, the passives only near their chips. */\n')
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
    out.write('};\n\nconst board_data_t bd_c16 = {\n    { %.3ff, %.3ff, %.3ff, %.3ff },\n' % OUTLINE)
    out.write('    nets, %d, pads, %d, comps, %d,\n};\n' % (len(names), first, len(order)))


if __name__ == '__main__':
    main()
