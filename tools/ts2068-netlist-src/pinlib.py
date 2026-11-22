"""Pinouts of the chips of the TS2068 (board 335-850001, schematic 335-870500).
'name:dir' tokens in pin order. dir: i input, o output, t tri-state output,
b bidirectional, c open collector, n no-connect, g ground, v +5V, x unchecked.
"""
def mk(s):
    return [tuple(t.split(':')) for t in s.split()]

PINS = {
 'Z80': mk('A11:t A12:t A13:t A14:t A15:t CLK:i D4:b D3:b D5:b D6:b VCC:v D2:b D7:b D0:b D1:b '
           'INT:i NMI:i HALT:o MREQ:t IORQ:t RD:t WR:t BUSAK:o WAIT:i BUSRQ:i RESET:i M1:o RFSH:o '
           'GND:g A0:t A1:t A2:t A3:t A4:t A5:t A6:t A7:t A8:t A9:t A10:t'),
 # SCLD, 68 pin PLCC: pin numbers as on the service data (U3 top view). Directions are
 # checked loosely (x) because the bus pins change direction.
 'SCLD': mk(' '.join('P%d:x' % i for i in range(1, 69))),
 'AY8912': mk('C:x TEST1:i VCC:v B:x A:x GND:g IOA7:b IOA6:b IOA5:b IOA4:b IOA3:b IOA2:b IOA1:b IOA0:b '
              'CLOCK:i RESET:i BDIR:i BC1:i BC2:i A8:i D7:b D6:b D5:b D4:b D3:b D2:b D1:b D0:b'),
 'ROM23128': mk('NC:n A12:i A7:i A6:i A5:i A4:i A3:i A2:i A1:i A0:i D0:t D1:t D2:t GND:g '
                'D3:t D4:t D5:t D6:t D7:t CS:i A10:i OE:i A11:i A9:i A8:i A13:i OEP:i VCC:v'),
 'ROM2364': mk('NC:n A12:i A7:i A6:i A5:i A4:i A3:i A2:i A1:i A0:i D0:t D1:t D2:t GND:g '
               'D3:t D4:t D5:t D6:t D7:t CS:i A10:i OE:i A11:i A9:i A8:i NC2:n NC3:n VCC:v'),
 '4416': mk('OE:i D1:i D2:i WE:i RAS:i A6:i A5:i A4:i VCC:v A7:i A3:i A2:i A1:i A0:i D3:i CAS:i D4:t GND:g'),
 'LS245': mk('DIR:i A1:b A2:b A3:b A4:b A5:b A6:b A7:b A8:b GND:g B8:b B7:b B6:b B5:b B4:b B3:b B2:b B1:b E:i VCC:v'),
 'LS244': mk('1G:i 1A1:i 2Y4:t 1A2:i 2Y3:t 1A3:i 2Y2:t 1A4:i 2Y1:t GND:g 2A1:i 1Y4:t 2A2:i 1Y3:t 2A3:i 1Y2:t 2A4:i 1Y1:t 2G:i VCC:v'),
 'LS157': mk('S:i 1A:i 1B:i 1Y:o 2A:i 2B:i 2Y:o GND:g 3Y:o 3B:i 3A:i 4Y:o 4B:i 4A:i G:i VCC:v'),
 'LS00':  mk('1A:i 1B:i 1Y:o 2A:i 2B:i 2Y:o GND:g 3Y:o 3A:i 3B:i 4Y:o 4A:i 4B:i VCC:v'),
 'DIODE': mk('A:x K:x'), 'RES': mk('P1:x P2:x'), 'CAP': mk('P1:x P2:x'), 'XTAL': mk('P1:x P2:x'),
 'CONN':  mk(' '.join('P%d:x' % i for i in range(1, 65))),
 'Q3': mk('B:x C:x E:x'),
}
PWR = {
 'Z80': {11: '+5V', 29: 'GND'},
 'AY8912': {3: '+5V', 6: 'GND'},
 'ROM23128': {28: '+5V', 14: 'GND'},
 'ROM2364': {28: '+5V', 14: 'GND'},
 '4416': {9: '+5V', 18: 'GND'},
}
