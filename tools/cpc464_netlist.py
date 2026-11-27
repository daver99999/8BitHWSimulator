#!/usr/bin/env python3
"""cpc464_netlist.py - the netlist of the Amstrad CPC 464 main board (PCB Z70100, 1983), typed in from the CPC 464 circuit diagram
(redraw after the Amstrad service manual, V1.04 2023-11-16 by SerErris, cpcwiki.eu).  Writes tools/cpc464-netlist-src/cpc464.net in the format the
other boards use (chip REF TYPE|VALUE|PINS, net NAME: REF.PIN ...).  Where a connection could not be read with certainty it is in UNSURE
(a beep-check item, see docs/CPC464_BEEP_CHECKS.md).

Part references are the board's own (IC101 ... IC125, R101 ..., C101 ..., J101 ...).  The schematic calls the transistors Q101-Q103, the board
silk screen TR1-TR3 (to be matched by beep).  Connector pins are numbered as on the schematic; the tape port pins are A to H.
Usage: tools/cpc464_netlist.py [out.net]"""
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


GND, V5 = 'GND', '+5V'
A = ['A%d' % i for i in range(16)]
D = ['D%d' % i for i in range(8)]

# ----------------------------------------------------------------------------------------------------------------------
# Z80A CPU IC111
part('IC111', 'Z80', 40, 'Z80A CPU')
Z80A = [30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 1, 2, 3, 4, 5]
Z80D = [14, 15, 12, 8, 7, 9, 10, 13]
for i in range(16):
    net(A[i], 'IC111.%d' % Z80A[i])
for i in range(8):
    net(D[i], 'IC111.%d' % Z80D[i])
net('INT', 'IC111.16'); net('NMI', 'IC111.17'); net('HALT', 'IC111.18'); net('MREQ', 'IC111.19'); net('IORQ', 'IC111.20')
net('RD', 'IC111.21'); net('WR', 'IC111.22'); net('BUSAK', 'IC111.23'); net('READY', 'IC111.24'); net('BUSRQ', 'IC111.25')
net('RESET', 'IC111.26'); net('M1', 'IC111.27'); net('RFSH', 'IC111.28'); net('CLKPHI', 'IC111.6')
net(V5, 'IC111.11'); net(GND, 'IC111.29')
part('R109', 'R', 2, '2.2K'); net(V5, 'R109.1'); net('NMI', 'R109.2')
part('R110', 'R', 2, '2.2K'); net(V5, 'R110.1'); net('BUSRQ', 'R110.2')

# ----------------------------------------------------------------------------------------------------------------------
# ROM IC103 (32K byte, 200 ns, TMM 232561 / the Amstrad 40009): 27256 pin-out
part('IC103', 'ROM32K', 28, 'Amstrad 40009 32K ROM')
ROMA = {14: 27, 13: 26, 12: 2, 11: 23, 10: 21, 9: 24, 8: 25, 7: 3, 6: 4, 5: 5, 4: 6, 3: 7, 2: 8, 1: 9, 0: 10}
for a, p in ROMA.items():
    net('A%d' % a, 'IC103.%d' % p)
ROMD = [11, 12, 13, 15, 16, 17, 18, 19]
for i in range(8):
    net(D[i], 'IC103.%d' % ROMD[i])
net('ROMEN', 'IC103.20')
net('ROMDIS', 'IC103.22')
part('R112', 'R', 2, '2.2K'); net('ROMDIS', 'R112.1'); net(GND, 'R112.2')
net(V5, 'IC103.28'); net(GND, 'IC103.14')
unsure('IC103 ROM: the drawing labels only the first address line of each group (A14, A11, A7, A3, A0); the other lines are taken from the 27256 pin-out. Pin 1 (N.C.) is not connected. Which ROM chip it really is (the Amstrad 40009 on this board) is the owner.')

# ----------------------------------------------------------------------------------------------------------------------
# PPI IC107 (M5L8255AP), AY-3-8912 IC102, 74LS145 IC101, keyboard CP002, joystick J102, tape CP001, sound mixer J103
part('IC107', 'PPI8255', 40, 'M5L8255AP PPI')
PPID = [34, 33, 32, 31, 30, 29, 28, 27]
for i in range(8):
    net(D[i], 'IC107.%d' % PPID[i])
net('A8', 'IC107.9'); net('A9', 'IC107.8'); net('A11', 'IC107.6'); net('IORD', 'IC107.5'); net('IOWR', 'IC107.36')
net('RESETH', 'IC107.35')
net(V5, 'IC107.26'); net(GND, 'IC107.7')
# port A to the AY data bus, port C low nibble to the keyboard decoder, PC7 / PC6 to BDIR / BC1, PC5 tape data, PC4 motor
for pa, ay in zip([4, 3, 2, 1, 40, 39, 38, 37], [28, 27, 26, 25, 24, 23, 22, 21]):
    net('PSG_D%d' % (28 - ay), 'IC107.%d' % pa, 'IC102.%d' % ay)
net('BDIR', 'IC107.10', 'IC102.18'); net('BC1', 'IC107.11', 'IC102.20')
net('WRDATA', 'IC107.12'); net('MOTORB', 'IC107.13')
for pc, p145 in zip([14, 15, 16, 17], [15, 14, 13, 12]):
    net('KBD_%s' % 'ABCD'[[14, 15, 16, 17].index(pc)], 'IC107.%d' % pc, 'IC101.%d' % p145)
# port B: VSYNC, OS links LK1-LK4, EXP, BUSY, tape read data
net('VSYNC', 'IC107.18')
net('PB1', 'IC107.19'); net('PB2', 'IC107.20'); net('PB3', 'IC107.21'); net('PB4', 'IC107.22')
net('EXP', 'IC107.23'); net('BUSY', 'IC107.24'); net('RDDATA', 'IC107.25')
for lk, pb in zip(['LK1', 'LK2', 'LK3', 'LK4'], ['PB1', 'PB2', 'PB3', 'PB4']):
    part(lk, 'LINK', 2, 'OS link')
    net(pb, lk + '.1'); net(GND, lk + '.2')
for r, n in (('R103', 'BUSY'), ('R104', 'EXP'), ('R105', 'PB4'), ('R106', 'PB3'), ('R107', 'PB2'), ('R108', 'PB1')):
    part(r, 'R', 2, '2.2K'); net(V5, r + '.1'); net(n, r + '.2')
unsure('OWNER BOARD (beeps 20-23): LK1-LK4 all read OPEN. LK1-LK4 (the OS links): the schematic shows each as a pad pair from the port B line to ground; whether a link is fitted on the owner board is a beep check.')

part('IC102', 'AY8912', 28, 'AY-3-8912 sound')
net(V5, 'IC102.3', 'IC102.17', 'IC102.19'); net(GND, 'IC102.6')
net('RESET', 'IC102.16'); net('CPUADDR', 'IC102.15')
for i, p in enumerate([14, 13, 12, 11, 10, 9, 8, 7]):
    net('IOA%d' % i, 'IC102.%d' % p)
net('SND_A', 'IC102.5'); net('SND_B', 'IC102.4'); net('SND_C', 'IC102.1')
unsure('IC102 pin 2 (TEST 2) is marked not connected; pins 17 (A8) and 19 (BC2) are taken to +5 V.')

part('IC101', 'LS145', 16, '74LS145 keyboard row decoder')
net(V5, 'IC101.16'); net(GND, 'IC101.8')
# outputs 0..9 = pins 1,2,3,4,5,6,7,9,10,11 -> keyboard connector Y1..Y10 = CP002 pins 19,18,17,16,15,14,13,12,11,2
part('CP002', 'CONN', 20, 'keyboard connector')
for p145, cp in zip([1, 2, 3, 4, 5, 6, 7, 9, 10, 11], [19, 18, 17, 16, 15, 14, 13, 12, 11, 2]):
    net('KY%d' % (20 - cp if cp > 11 else (10 if cp == 2 else 9)), 'IC101.%d' % p145, 'CP002.%d' % cp)
part('J102', 'CONN', 9, 'joystick / paddle port (9 pin D)')
# AY port A bits go to the keyboard columns and to the joystick
for i, (kp, jp) in enumerate([(3, 1), (4, 2), (5, 3), (6, 4), (7, 6), (8, 7), (9, 5)]):
    net('IOA%d' % i, 'CP002.%d' % kp, 'J102.%d' % jp)
net('IOA7', 'CP002.10', 'CP002.1')
# joystick commons: Y10 (CP002.2) = COMMON (J102.8), Y7 (CP002.13) = COM 2 (J102.9)
net('KY10', 'J102.8'); net('KY7', 'J102.9')
unsure('Keyboard connector: IOA7 (AY pin 7) feeds both CP002 pins 10 and 1; Y10 is shared with J102 pin 8 (COMMON) and Y7 with J102 pin 9 (COM 2). Taken from the trace of the drawing.')

part('CP001', 'CONN', 8, 'tape port (A-H = 1-8)')
net(V5, 'CP001.A', 'CP001.C'); net(GND, 'CP001.B'); net('WRDATA', 'CP001.D'); net('RDDATA', 'CP001.E'); net('SOUND', 'CP001.G'); net('MOTOR', 'CP001.H')
part('Q101', 'NPN', 3, '2SC2120 motor switch'); net('MOTOR', 'Q101.C'); net(GND, 'Q101.E'); net('MOTORB1', 'Q101.B')
part('R113', 'R', 2, '1K'); net('MOTORB', 'R113.1'); net('MOTORB1', 'R113.2')

# sound mixer J103 (stereo jack) and the EXP SOUND line
part('J103', 'CONN', 3, 'stereo sound jack')
net(GND, 'J103.1'); net('SND_L', 'J103.3', 'R117.2', 'R118.2'); net('SND_R', 'J103.2', 'R119.2', 'R120.2')
for r, a_, b_, v in (('R114', 'SOUND', 'SND_C', '10k'), ('R115', 'SOUND', 'SND_B', '10k'), ('R116', 'SOUND', 'SND_A', '10k'),
                     ('R117', 'SND_C', 'SND_L', '10K'), ('R118', 'SND_B', 'SND_L', '22K'), ('R119', 'SND_B', 'SND_R', '22K'),
                     ('R120', 'SND_A', 'SND_R', '10K'), ('R121', 'SND_C', GND, '1K'), ('R122', 'SND_B', GND, '1K'), ('R123', 'SND_A', GND, '1K')):
    part(r, 'R', 2, v)
    net(a_, r + '.1')
    net(b_, r + '.2')
unsure('Sound mixer: channel C goes to the left output through R117, B to both through R118 and R119, A to the right through R120 (read from the drawing); the three 10k R114-R116 join the channels to the SOUND line (tape port G and expansion pin 1).')

# ----------------------------------------------------------------------------------------------------------------------
# reset logic, IC110 74LS132 (gates A, B, C, D), IC112 74LS32 (gates A, B, C, D)
part('IC110', 'LS132', 14, '74LS132'); net(V5, 'IC110.14'); net(GND, 'IC110.7')
part('IC112', 'LS32', 14, '74LS32'); net(V5, 'IC112.14'); net(GND, 'IC112.7')
part('R101', 'R', 2, '22k'); net(V5, 'R101.1'); net('RC', 'R101.2')
part('D101', 'D', 2, '1S2472'); net('RC', 'D101.A'); net(V5, 'D101.K')
part('C102', 'CE', 2, '10V47'); net('RC', 'C102.1'); net(GND, 'C102.2')
part('R102', 'R', 2, '2.2K'); net(V5, 'R102.1'); net('BUSRESET', 'R102.2')
net('RC', 'IC110.1'); net('BUSRESET', 'IC110.2'); net('RESETH', 'IC110.3')
net('RESETH', 'IC110.4', 'IC110.5'); net('RESET', 'IC110.6')
net('IOWR', 'IC110.9'); net('IORD', 'IC110.10'); net('CRTC_E', 'IC110.8')
net('PRSTRB', 'IC110.11'); net('PRQ7', 'IC110.12', 'IC110.13')
net('WR', 'IC112.1'); net('IORQ', 'IC112.2'); net('IOWR', 'IC112.3')
net('IORQ', 'IC112.4'); net('RD', 'IC112.5'); net('IORD', 'IC112.6')
net('A12', 'IC112.12'); net('IOWR', 'IC112.13'); net('PRCK', 'IC112.11')
net('RAMRD', 'IC112.10'); net('RAMDIS', 'IC112.9'); net('IC114OE', 'IC112.8')
part('R124', 'R', 2, '2.2K'); net('RAMDIS', 'R124.1'); net(GND, 'R124.2')
unsure('IC110 / IC112 gate pin numbers read from the drawing: IC110 A = 1,2 to 3, B = 4,5 to 6, C = 9,10 to 8, D = 12,13 to 11; IC112 A = 1,2 to 3, B = 4,5 to 6, C = 12,13 to 11, D = 9,10 to 8.')

# ----------------------------------------------------------------------------------------------------------------------
# printer port: IC106 74LS273 latch, edge connector PRN (36 way)
part('IC106', 'LS273', 20, '74LS273 printer latch')
net(V5, 'IC106.20'); net(GND, 'IC106.10'); net('RESET', 'IC106.1'); net('PRCK', 'IC106.11')
for i, (pd, pq) in enumerate(zip([3, 4, 7, 8, 13, 14, 17, 18], [2, 5, 6, 9, 12, 15, 16, 19])):
    net(D[i], 'IC106.%d' % pd); net('PRQ%d' % i, 'IC106.%d' % pq)
part('PRN', 'CONN', 36, 'printer port edge connector')
net('PRSTRB', 'PRN.1')
for i in range(7):
    net('PRQ%d' % i, 'PRN.%d' % (2 + i))
net(GND, 'PRN.9')
net('BUSY', 'PRN.11')
for p in (14, 16, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 33):
    net(GND, 'PRN.%d' % p)
unsure('IC106: the drawing numbers the clock input 14; a 74LS273 has its clock on pin 11, which is used here. Printer connector pin 9 (D7) is grounded on the drawing; the printer strobe is bit 7 of the latch through IC110 gate D.')

# ----------------------------------------------------------------------------------------------------------------------
# CRTC IC108 HD6845SP
part('IC108', 'CRTC6845', 40, 'HD6845SP CRTC')
net(V5, 'IC108.20'); net(GND, 'IC108.1'); net('RESET', 'IC108.2')
for i in range(8):
    net(D[i], 'IC108.%d' % (33 - i))
net('A14', 'IC108.25'); net('A9', 'IC108.22'); net('A8', 'IC108.24'); net('CRTC_E', 'IC108.23')
net('LPEN', 'IC108.3'); net('CURSOR', 'IC108.19')
net('DISPEN', 'IC108.18'); net('HSYNC', 'IC108.39'); net('VSYNC', 'IC108.40'); net('CCLK', 'IC108.21')
for i, p in enumerate([4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17]):
    net('MA%d' % i, 'IC108.%d' % p)
for i, p in enumerate([38, 37, 36, 35, 34]):
    net('RA%d' % i, 'IC108.%d' % p)
part('R142', 'R', 2, '10K'); net('LPEN', 'R142.1'); net(GND, 'R142.2')
part('PAD', 'PAD', 1, 'test pad on MA11'); net('MA11', 'PAD.1')
unsure('IC108 MA10 (pin 14), RA3 (35) and RA4 (34) are marked not connected; MA11 (15) goes to a test pad only.')

# address multiplexers IC109 / IC104 / IC113 / IC105 (74LS153): B select = CPU ADDR, A select = CAS ADDR, /G to ground
MUX = {
    'IC109': {10: 'A15', 6: 'A14', 11: 'A8', 5: 'A7', 12: 'MA13', 4: 'MA12', 13: 'MA7', 3: 'MA6'},
    'IC104': {10: 'A13', 6: 'A12', 11: 'A6', 5: 'A5', 12: 'RA2', 4: 'RA1', 13: 'MA5', 3: 'MA4'},
    'IC113': {10: 'A11', 6: 'A10', 11: 'A4', 5: 'A3', 12: 'RA0', 4: 'MA9', 13: 'MA3', 3: 'MA2'},
    'IC105': {10: 'A9', 6: 'A0', 11: 'A2', 5: 'A1', 12: 'MA8', 4: 'CCLK', 13: 'MA1', 3: 'MA0'},
}
for ic, mp in MUX.items():
    part(ic, 'LS153', 16, '74LS153 address mux')
    net(V5, ic + '.16'); net(GND, ic + '.8', ic + '.1', ic + '.15')
    net('CPUADDR', ic + '.2'); net('CASADDR', ic + '.14')
    for p, n in mp.items():
        net(n, '%s.%d' % (ic, p))
# mux outputs (1Y = pin 7, 2Y = pin 9) -> 120 ohm -> DRAM address pins
RAMA = {5: 'R149', 7: 'R148', 6: 'R153', 12: 'R152', 11: 'R147', 10: 'R146', 13: 'R151', 9: 'R150'}   # DRAM pin -> series resistor
MUXOUT = {'R150': 'IC109.7', 'R151': 'IC109.9', 'R146': 'IC104.7', 'R147': 'IC104.9', 'R152': 'IC113.7', 'R153': 'IC113.9', 'R148': 'IC105.7', 'R149': 'IC105.9'}
RAMNET = {5: 'RAM_A0', 7: 'RAM_A1', 6: 'RAM_A2', 12: 'RAM_A3', 11: 'RAM_A4', 10: 'RAM_A5', 13: 'RAM_A6', 9: 'RAM_A7'}
for p, r in RAMA.items():
    part(r, 'R', 2, '120')
    net('MUX_' + RAMNET[p][4:], r + '.1', MUXOUT[r])
    net(RAMNET[p], r + '.2')
    for k in range(8):
        net(RAMNET[p], 'IC%d.%d' % ((124, 123, 122, 121, 117, 118, 119, 120)[k], p))

# DRAMs IC124 ... IC120: data bit D7 ... D0 (top to bottom of the drawing)
RAMIC = (124, 123, 122, 121, 117, 118, 119, 120)
for k, n in enumerate(RAMIC):
    ref = 'IC%d' % n
    bit = 7 - k
    part(ref, 'DRAM4164', 16, 'uPD4164-2 64Kx1')
    net(D[bit], ref + '.2')                  # DIN from the CPU data bus
    net('RD%d' % bit, ref + '.14')           # DOUT on the RAM data line
    net('RAS', ref + '.4'); net('CAS', ref + '.15'); net('MWE', ref + '.3')
    net(V5, ref + '.8'); net(GND, ref + '.16')
unsure('DRAM bit order: the drawing lists IC124 at the top (data in D7) down to IC120 (D0); the data-out lines are taken to the gate array pins 31 to 24 in the same order.')
unsure('DRAM pin 1 (N.C. on the 4164) is shown with a cross; the 120 ohm resistors R146-R153 sit between the multiplexer outputs and the DRAM address pins (order checked against the stubs on the drawing).')

# data buffers IC115 (74LS244 CPU to RAM data), IC114 (74LS373 RAM data to CPU) and the gate array IC116
part('IC115', 'LS244', 20, '74LS244 data buffer'); net(V5, 'IC115.20'); net(GND, 'IC115.10')
net('N244EN', 'IC115.1', 'IC115.19')
for i, (pi, po) in enumerate(zip([2, 4, 6, 8, 17, 15, 13, 11], [18, 16, 14, 12, 3, 5, 7, 9])):   # D0 ... D7: in 2,4,6,8,17,15,13,11
    net(D[i], 'IC115.%d' % pi); net('RD%d' % i, 'IC115.%d' % po)
part('IC114', 'LS373', 20, '74LS373 data latch'); net(V5, 'IC114.20'); net(GND, 'IC114.10')
net('IC114OE', 'IC114.1'); net('READY', 'IC114.11')
for i, (pd, pq) in enumerate(zip([3, 4, 7, 8, 13, 14, 17, 18], [2, 5, 6, 9, 12, 15, 16, 19])):
    net('RD%d' % i, 'IC114.%d' % pd); net(D[i], 'IC114.%d' % pq)

part('IC116', 'GATEARRAY', 40, 'Amstrad 40007 gate array')
net('CPUADDR', 'IC116.1'); net('READY', 'IC116.2'); net('CAS', 'IC116.3'); net('N244EN', 'IC116.4'); net('MWE', 'IC116.5')
net('CASADDR', 'IC116.6'); net('RAS', 'IC116.7'); net('XTAL', 'IC116.8'); net('VCC2', 'IC116.9'); net('INT', 'IC116.10')
net('SYNC', 'IC116.11'); net('ROMEN', 'IC116.12'); net('RAMRD', 'IC116.13'); net('HSYNC', 'IC116.14'); net('VSYNC', 'IC116.15')
net('IORQ', 'IC116.16'); net('M1', 'IC116.17'); net('MREQ', 'IC116.18'); net('RD', 'IC116.19'); net('A15', 'IC116.20'); net('A14', 'IC116.21')
net(V5, 'IC116.22'); net('DISPEN', 'IC116.23')
for i, p in enumerate([24, 25, 26, 27, 28, 29, 30, 31]):
    net('RD%d' % i, 'IC116.%d' % p)
net('GA_B', 'IC116.32'); net('VCC2', 'IC116.33'); net('GA_G', 'IC116.34'); net(GND, 'IC116.35'); net('GA_R', 'IC116.36')
net('RESET', 'IC116.37'); net(V5, 'IC116.38'); net('PHI', 'IC116.39'); net('CCLK', 'IC116.40')
part('R138', 'R', 2, '12'); net('VCC2', 'R138.1'); net(V5, 'R138.2')
part('R139', 'R', 2, '12'); net('VCC2', 'R139.1'); net(V5, 'R139.2')
part('C104', 'C', 2, '0.1'); net(V5, 'C104.1'); net(GND, 'C104.2')
unsure('IC116 (the gate array, unmarked on the owner board, the heat-sunk chip): the pin out is taken from the drawing: R 36, G 34, B 32, SYNC 11, XTAL 8, VCC2 33 and 9 (through R138 / R139 12 ohm), VCC1 38 and 22, GND 35.')

# system clock: crystal X101 and the four NAND gates IC125 (ZN7400E)
part('IC125', 'ZN7400', 14, 'ZN7400E'); net(V5, 'IC125.14'); net(GND, 'IC125.7')
part('X101', 'XTAL', 2, '16.0MHz'); part('C103', 'C', 2, '22P'); part('C129', 'C', 2, '100P'); part('C130', 'C', 2, '68P')
part('R143', 'R', 2, '330'); part('R144', 'R', 2, '330'); part('R145', 'R', 2, '1K'); part('RX1', 'R', 2, '56')
net('XTAL', 'IC125.8', 'IC116.8', 'R145.2'); net(V5, 'R145.1')
net('OSC1', 'IC125.9', 'IC125.10', 'IC125.11', 'C130.1'); net(GND, 'C130.2')
net('OSC2', 'IC125.12', 'IC125.13', 'IC125.6', 'X101.1', 'R144.1')
net('OSC3', 'IC125.4', 'IC125.5', 'R144.2', 'C129.1', 'RX1.1')
net('OSC4', 'IC125.3', 'C129.2', 'RX1.2', 'R143.1')
net('OSC5', 'IC125.1', 'IC125.2', 'R143.2', 'C103.2'); net('OSC6', 'X101.2', 'C103.1')
unsure('Crystal oscillator: gate 1 (pins 9, 10 in, 8 out) feeds XTAL; gate 2 (12, 13 in, 11 out); gate 3 (4, 5 in, 6 out); gate 4 (1, 2 in, 3 out). Gate 2 output (11) goes to gate 1 inputs (9, 10) with C130, X101 / C103 as in the drawing. The 56 ohm resistor has no reference on the drawing (called RX1).')

# 4 MHz clock driver: Q103 ZTX312L
part('Q103', 'NPN', 3, 'ZTX312L clock driver'); net('PHI', 'Q103.B'); net(GND, 'Q103.E'); net('QC', 'Q103.C')
part('R140', 'R', 2, '100'); net(V5, 'R140.1'); net('QC', 'R140.2')
part('R141', 'R', 2, '68'); net('QC', 'R141.1'); net('CLKPHI', 'R141.2')

# video output: R125-R137, Q102, J101 (6 pin DIN)
part('J101', 'CONN', 6, 'video output DIN')
for col, ga, rp, rs, jp in (('R', 'GA_R', 'R125', 'R126', 1), ('G', 'GA_G', 'R127', 'R128', 2), ('B', 'GA_B', 'R129', 'R130', 3)):
    part(rp, 'R', 2, '680'); net(V5, rp + '.1'); net(ga, rp + '.2')
    part(rs, 'R', 2, '220'); net(ga, rs + '.1'); net('J101_%d' % jp, rs + '.2', 'J101.%d' % jp)
part('R131', 'R', 2, '3.3K'); net('GA_R', 'R131.1'); net('LUMB', 'R131.2')
part('R132', 'R', 2, '1K'); net('GA_G', 'R132.1'); net('LUMB', 'R132.2')
part('R133', 'R', 2, '10K'); net('GA_B', 'R133.1'); net('LUMB', 'R133.2')
part('R135', 'R', 2, '680'); net('LUMB', 'R135.1'); net('SYNC', 'R135.2')
part('Q102', 'NPN', 3, '2SC1815 luminance'); net('LUMB', 'Q102.B'); net(V5, 'Q102.C'); net('LUME', 'Q102.E')
part('R134', 'R', 2, '100'); net('LUME', 'R134.1'); net('LUM', 'R134.2', 'J101.6')
part('R136', 'R', 2, '220'); net('LUM', 'R136.1'); net(GND, 'R136.2')
part('R137', 'R', 2, '220'); net('SYNC', 'R137.1'); net('J101_4', 'R137.2', 'J101.4')
net(GND, 'J101.5')

# power: DC jack and decoupling
part('J104', 'CONN', 3, '5VDC power jack')
net(V5, 'J104.1'); net(GND, 'J104.3')
part('C101', 'CE', 2, '10V47'); net(V5, 'C101.1'); net(GND, 'C101.2')
for n in range(105, 129):
    part('C%d' % n, 'C', 2, '0.1'); net(V5, 'C%d.1' % n); net(GND, 'C%d.2' % n)
unsure('Decoupling C105-C128 (0.1 uF x 24): the drawing lists them as a group, so they are netted straight across +5 V and ground. J104 pin 2 (jack switch) is not connected.')

# expansion connector (50 way edge connector EXP)
part('EXPC', 'CONN', 50, 'expansion connector')
EXP = ['SOUND', 'GND'] + ['A%d' % i for i in range(15, -1, -1)] + ['D%d' % i for i in range(7, -1, -1)] + [
    '+5V', 'MREQ', 'M1', 'RFSH', 'IORQ', 'RD', 'WR', 'HALT', 'INT', 'NMI', 'BUSRQ', 'BUSAK', 'READY', 'BUSRESET', 'RESET', 'ROMEN',
    'ROMDIS', 'RAMRD', 'RAMDIS', 'CURSOR', 'LPEN', 'EXP', 'GND', 'CLKPHI']
for i, n in enumerate(EXP):
    net(n, 'EXPC.%d' % (i + 1))

# ----------------------------------------------------------------------------------------------------------------------
def write(out):
    with open(out, 'w') as f:
        for ref in sorted(chips, key=lambda r: (r.rstrip('0123456789'), int(''.join(c for c in r if c.isdigit()) or 0))):
            typ, val, n = chips[ref]
            f.write('chip %s %s|%s|%d\n' % (ref, typ, val, n))
        for name in sorted(nets):
            if nets[name]:
                f.write('net %s: %s\n' % (name, ' '.join(nets[name])))


if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, 'cpc464-netlist-src', 'cpc464.net')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    # a part pin must appear in only one net
    seen = {}
    bad = 0
    for name, pins in nets.items():
        for p in pins:
            if p in seen and seen[p] != name:
                print('PIN IN TWO NETS: %s in %s and %s' % (p, seen[p], name)); bad += 1
            seen[p] = name
    write(out)
    print('%s: %d parts, %d nets, %d errors' % (out, len(chips), len([n for n in nets if nets[n]]), bad))
    for u in UNSURE:
        print('  UNSURE:', u)
