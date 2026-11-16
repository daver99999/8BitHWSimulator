"""Pinouts of the chips of the Sinclair ZX80.
'name:dir' tokens in pin order. dir: i input, o output, t tri-state output, b bidirectional, c open collector,
n no-connect, g ground, v +5V, x unchecked.
"""
def mk(s):
    return [tuple(t.split(':')) for t in s.split()]

G4 = '{0}:i {0}:i {0}:o'   # unused, written out below
PINS = {
 'Z80': mk('A11:t A12:t A13:t A14:t A15:t CLK:i D4:b D3:b D5:b D6:b VCC:v D2:b D7:b D0:b D1:b '
           'INT:i NMI:i HALT:o MREQ:t IORQ:t RD:t WR:t BUSAK:o WAIT:i BUSRQ:i RESET:i M1:o RFSH:o '
           'GND:g A0:t A1:t A2:t A3:t A4:t A5:t A6:t A7:t A8:t A9:t A10:t'),
 # IC2: the 8332 4K ROM (CS1 pin 20, CS2 pin 21 wired to A12); the 8K 2364 ZX81 ROM has the same pinout (pin 21 = A12)
 'ROM8332': mk('A7:i A6:i A5:i A4:i A3:i A2:i A1:i A0:i D0:t D1:t D2:t GND:g D3:t D4:t D5:t D6:t D7:t '
               'A11:i A10:i CS1:i CS2:i A9:i A8:i VCC:v'),
 'RAM2114': mk('A6:i A5:i A4:i A3:i A0:i A1:i A2:i CS:i GND:g WE:i IO4:b IO3:b IO2:b IO1:b A9:i A8:i A7:i VCC:v'),
 'LS373': mk('OE:i Q0:t D0:i D1:i Q1:t Q2:t D2:i D3:i Q3:t GND:g LE:i Q4:t D4:i D5:i Q5:t Q6:t D6:i D7:i Q7:t VCC:v'),
 'LS157': mk('S:i 1A:i 1B:i 1Y:o 2A:i 2B:i 2Y:o GND:g 3Y:o 3B:i 3A:i 4Y:o 4B:i 4A:i G:i VCC:v'),
 'LS165': mk('SHLD:i CK:i E:i F:i G:i H:i QHN:o GND:g QH:o SER:i A:i B:i C:i D:i CKINH:i VCC:v'),
 'LS365': mk('G1:i A1:i Y1:t A2:i Y2:t A3:i Y3:t GND:g Y4:t A4:i Y5:t A5:i Y6:t A6:i G2:i VCC:v'),
 'LS00': mk('1A:i 1B:i 1Y:o 2A:i 2B:i 2Y:o GND:g 3Y:o 3A:i 3B:i 4Y:o 4A:i 4B:i VCC:v'),
 'LS04': mk('1A:i 1Y:o 2A:i 2Y:o 3A:i 3Y:o GND:g 4Y:o 4A:i 5Y:o 5A:i 6Y:o 6A:i VCC:v'),
 'LS05': mk('1A:i 1Y:c 2A:i 2Y:c 3A:i 3Y:c GND:g 4Y:c 4A:i 5Y:c 5A:i 6Y:c 6A:i VCC:v'),
 'LS10': mk('1A:i 1B:i 2A:i 2B:i 2C:i 2Y:o GND:g 3Y:o 3A:i 3B:i 3C:i 1Y:o 1C:i VCC:v'),
 'LS32': mk('1A:i 1B:i 1Y:o 2A:i 2B:i 2Y:o GND:g 3Y:o 3A:i 3B:i 4Y:o 4A:i 4B:i VCC:v'),
 'LS74': mk('1CLR:i 1D:i 1CK:i 1PR:i 1Q:o 1QN:o GND:g 2QN:o 2Q:o 2PR:i 2CK:i 2D:i 2CLR:i VCC:v'),
 'LS86': mk('1A:i 1B:i 1Y:o 2A:i 2B:i 2Y:o GND:g 3Y:o 3A:i 3B:i 4Y:o 4A:i 4B:i VCC:v'),
 'LS93': mk('CKB:i R01:i R02:i NC4:n VCC:v NC6:n NC7:n QC:o QB:o GND:g QD:o QA:o NC13:n CKA:i'),
 'DIODE': mk('A:x K:x'), 'RES': mk('P1:x P2:x'), 'CAP': mk('P1:x P2:x'), 'XTAL': mk('P1:x P2:x'),
 'CONN': mk(' '.join('P%d:x' % i for i in range(1, 9))),
}
PWR = {
 'Z80': {11: '+5V', 29: 'GND'}, 'ROM8332': {24: '+5V', 12: 'GND'}, 'RAM2114': {18: '+5V', 9: 'GND'},
 'LS373': {20: '+5V', 10: 'GND'}, 'LS157': {16: '+5V', 8: 'GND'}, 'LS165': {16: '+5V', 8: 'GND'}, 'LS365': {16: '+5V', 8: 'GND'},
 'LS00': {14: '+5V', 7: 'GND'}, 'LS04': {14: '+5V', 7: 'GND'}, 'LS05': {14: '+5V', 7: 'GND'}, 'LS10': {14: '+5V', 7: 'GND'},
 'LS32': {14: '+5V', 7: 'GND'}, 'LS74': {14: '+5V', 7: 'GND'}, 'LS86': {14: '+5V', 7: 'GND'}, 'LS93': {5: '+5V', 10: 'GND'},
}
