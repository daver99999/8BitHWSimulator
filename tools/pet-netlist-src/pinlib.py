"""Pinout library for the chips on the PET 8032080-03 (4032) board.

Each type is a list of 'name:dir' tokens in pin order (pin 1 first).
dir: i input, o output, t tri-state output, b bidirectional, c open-collector
output, n no-connect, g ground, v +5V supply, x unchecked (analog / special),
a output that may legally share a net with other 'a' (wired bus).
The numbers drawn on the schematic are what we transcribe; this library is
only used to name pins and to catch errors (a pin that does not exist, an
input left floating, two outputs on one net).
"""

def mk(s):
    return [tuple(tok.split(':')) for tok in s.split()]

PINS = {
 # ---- gates (74LS/74S/74 share pinouts) ----
 'LS00': mk('1A:i 1B:i 1Y:o 2A:i 2B:i 2Y:o GND:g 3Y:o 3A:i 3B:i 4Y:o 4A:i 4B:i VCC:v'),
 'LS02': mk('1Y:o 1A:i 1B:i 2Y:o 2A:i 2B:i GND:g 3A:i 3B:i 3Y:o 4A:i 4B:i 4Y:o VCC:v'),
 'LS04': mk('1A:i 1Y:o 2A:i 2Y:o 3A:i 3Y:o GND:g 4Y:o 4A:i 5Y:o 5A:i 6Y:o 6A:i VCC:v'),
 'S04':  mk('1A:i 1Y:o 2A:i 2Y:o 3A:i 3Y:o GND:g 4Y:o 4A:i 5Y:o 5A:i 6Y:o 6A:i VCC:v'),
 '7417': mk('1A:i 1Y:c 2A:i 2Y:c 3A:i 3Y:c GND:g 4Y:c 4A:i 5Y:c 5A:i 6Y:c 6A:i VCC:v'),
 'LS08': mk('1A:i 1B:i 1Y:o 2A:i 2B:i 2Y:o GND:g 3Y:o 3A:i 3B:i 4Y:o 4A:i 4B:i VCC:v'),
 'LS10': mk('1A:i 1B:i 2A:i 2B:i 2C:i 2Y:o GND:g 3Y:o 3A:i 3B:i 3C:i 1Y:o 1C:i VCC:v'),
 'LS20': mk('1A:i 1B:i NC:n 1C:i 1D:i 1Y:o GND:g 2Y:o 2A:i 2B:i NC:n 2C:i 2D:i VCC:v'),
 '7425': mk('1A:i 1B:i 1STB:i 1C:i 1D:i 1Y:o GND:g 2Y:o 2A:i 2B:i 2STB:i 2C:i 2D:i VCC:v'),
 'LS86': mk('1A:i 1B:i 1Y:o 2A:i 2B:i 2Y:o GND:g 3Y:o 3A:i 3B:i 4Y:o 4A:i 4B:i VCC:v'),
 # ---- flip-flops / counters / shifters ----
 'LS74': mk('1CLR:i 1D:i 1CK:i 1PRE:i 1Q:o 1QN:o GND:g 2QN:o 2Q:o 2PRE:i 2CK:i 2D:i 2CLR:i VCC:v'),
 'S74':  mk('1CLR:i 1D:i 1CK:i 1PRE:i 1Q:o 1QN:o GND:g 2QN:o 2Q:o 2PRE:i 2CK:i 2D:i 2CLR:i VCC:v'),
 'LS107':mk('1J:i 1QN:o 1Q:o 1K:i 2Q:o 2QN:o GND:g 2J:i 2CK:i 2CLR:i 2K:i 1CK:i 1CLR:i VCC:v'),
 'LS93': mk('CKB:i R01:i R02:i NC:n VCC:v NC:n NC:n QC:o QB:o GND:g QD:o QA:o NC:n CKA:i'),
 '74177':mk('LOAD:i QC:o C:i A:i QA:o CK2:i GND:g CK1:i QB:o B:i D:i QD:o CLR:i VCC:v'),
 'LS164':mk('A:i B:i QA:o QB:o QC:o QD:o GND:g CK:i CLR:i QE:o QF:o QG:o QH:o VCC:v'),
 '74166':mk('SER:i A:i B:i C:i D:i CKINH:i CK:i GND:g CLR:i E:i F:i G:i QH:o H:i SL:i VCC:v'),
 # ---- mux / decoders ----
 'LS157':mk('S:i 1A:i 1B:i 1Y:o 2A:i 2B:i 2Y:o GND:g 3Y:o 3B:i 3A:i 4Y:o 4B:i 4A:i G:i VCC:v'),
 'LS138':mk('A:i B:i C:i G2A:i G2B:i G1:i Y7:o GND:g Y6:o Y5:o Y4:o Y3:o Y2:o Y1:o Y0:o VCC:v'),
 'LS145':mk('Y0:c Y1:c Y2:c Y3:c Y4:c Y5:c Y6:c GND:g Y7:c Y8:c Y9:c D:i C:i B:i A:i VCC:v'),
 '74154':mk('Y0:o Y1:o Y2:o Y3:o Y4:o Y5:o Y6:o Y7:o Y8:o Y9:o Y10:o GND:g Y11:o Y12:o Y13:o Y14:o Y15:o G1:i G2:i D:i C:i B:i A:i VCC:v'),
 # ---- buffers / latches ----
 'LS244':mk('1G:i 1A1:i 2Y4:t 1A2:i 2Y3:t 1A3:i 2Y2:t 1A4:i 2Y1:t GND:g 2A1:i 1Y4:t 2A2:i 1Y3:t 2A3:i 1Y2:t 2A4:i 1Y1:t 2G:i VCC:v'),
 'LS373':mk('OC:i 1Q:t 1D:i 2D:i 2Q:t 3Q:t 3D:i 4D:i 4Q:t GND:g LE:i 5Q:t 5D:i 6D:i 6Q:t 7Q:t 7D:i 8D:i 8Q:t VCC:v'),
 # ---- memories ----
 '4116': mk('VBB:x DIN:i WE:i RAS:i A0:i A2:i A1:i VDD:x VCC:v A5:i A4:i A3:i A6:i DOUT:t CAS:i VSS:g'),
 '2114': mk('A6:i A5:i A4:i A3:i A0:i A1:i A2:i CS:i GND:g WE:i D3:b D2:b D1:b D0:b A9:i A8:i A7:i VCC:v'),
 '2332': mk('A7:i A6:i A5:i A4:i A3:i A2:i A1:i A0:i D0:t D1:t D2:t GND:g D3:t D4:t D5:t D6:t D7:t A11:i A10:i CS1:i CS2:i A9:i A8:i VCC:v'),
 '2316': mk('A7:i A6:i A5:i A4:i A3:i A2:i A1:i A0:i D0:t D1:t D2:t GND:g D3:t D4:t D5:t D6:t D7:t CS2:i A10:i CS1:i CS3:i A9:i A8:i VCC:v'),
 # ---- processors / peripherals ----
 '6502': mk('VSS:g RDY:i PHI1:o IRQ:i NC:n NMI:i SYNC:o VCC:v A0:t A1:t A2:t A3:t A4:t A5:t A6:t A7:t A8:t A9:t A10:t A11:t VSS2:g A12:t A13:t A14:t A15:t D7:b D6:b D5:b D4:b D3:b D2:b D1:b D0:b RW:t NC:x NC:x PHI0:i SO:i PHI2:o RES:i'),
 '6520': mk('VSS:g PA0:b PA1:b PA2:b PA3:b PA4:b PA5:b PA6:b PA7:b PB0:b PB1:b PB2:b PB3:b PB4:b PB5:b PB6:b PB7:b CB1:b CB2:b VCC:v RW:i CS0:i CS2:i CS1:i PHI2:i D7:b D6:b D5:b D4:b D3:b D2:b D1:b D0:b RES:i RS1:i RS0:i IRQB:c IRQA:c CA2:b CA1:b'),
 '6522': mk('VSS:g PA0:b PA1:b PA2:b PA3:b PA4:b PA5:b PA6:b PA7:b PB0:b PB1:b PB2:b PB3:b PB4:b PB5:b PB6:b PB7:b CB1:b CB2:b VCC:v IRQ:c RW:i CS2:i CS1:i PHI2:i D7:b D6:b D5:b D4:b D3:b D2:b D1:b D0:b RES:i RS3:i RS2:i RS1:i RS0:i CA2:b CA1:b'),
 'CRTC': mk('VSS:g RES:i LPSTB:i MA0:o MA1:o MA2:o MA3:o MA4:o MA5:o MA6:o MA7:o MA8:o MA9:o MA10:o MA11:o MA12:o MA13:o DE:o CURSOR:o VCC:v CLK:i RW:i E:i RS:i CS:i D7:b D6:b D5:b D4:b D3:b D2:b D1:b D0:b RA4:o RA3:o RA2:o RA1:o RA0:o HSYNC:o VSYNC:o'),
 'LM555':mk('GND:g TRIG:i OUT:o RESET:i CTRL:x THR:i DIS:x VCC:v'),
 'MC3446':mk('P1:b P2:b P3:b P4:b P5:b P6:b P7:b GND:g P9:b P10:b P11:b P12:b P13:b P14:b P15:b VCC:v'),
 # ---- passive / analog packages: pins unchecked ----
 'Q2T2222':mk(' '.join('P%d:x' % i for i in range(1, 15))),
 'RNET14': mk(' '.join('P%d:x' % i for i in range(1, 15))),
 'CONN':   mk(' '.join('P%d:x' % i for i in range(1, 121))),
 'Q3':     mk('B:x C:x E:x'),
 'XTAL':   mk('P1:x P2:x'),
 'RES':    mk('P1:x P2:x'),
 'BRIDGE': mk('P1:x P2:x P3:x P4:x'),
 'CAP':    mk('P1:x P2:x'),
}

# supply pins that are not the usual (type -> {pin: net})
PWR = {
 '4116':  {1: '-5V', 8: '+12V', 9: '+5V', 16: 'GND'},
 '6502':  {1: 'GND', 8: '+5V', 21: 'GND'},
}
