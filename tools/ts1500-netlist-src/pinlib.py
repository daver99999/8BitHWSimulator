"""Pinouts of the chips of the Timex Sinclair 1500.
'name:dir' tokens in pin order. dir: i input, o output, t tri-state output,
b bidirectional, c open collector, n no-connect, g ground, v +5V, x unchecked.
"""
def mk(s):
    return [tuple(t.split(':')) for t in s.split()]

PINS = {
 'Z80': mk('A11:t A12:t A13:t A14:t A15:t CLK:i D4:b D3:b D5:b D6:b VCC:v D2:b D7:b D0:b D1:b '
           'INT:i NMI:i HALT:o MREQ:t IORQ:t RD:t WR:t BUSAK:o WAIT:i BUSRQ:i RESET:i M1:o RFSH:o '
           'GND:g A0:t A1:t A2:t A3:t A4:t A5:t A6:t A7:t A8:t A9:t A10:t'),
 # U5, the 68 pin gate array. THE PIN NUMBERS ARE INFERRED (the schematic's are not legible, except 59 and 54): counted along
 # the symbol: left side from the top 59..43, bottom from the left 42..22, right side from the bottom 21..4, top from the right
 # 3, 2, 1, 68..60.
 'GA1500': mk('MREQ:i WR:i M1:i CLKN:o CLK:o CPUCLK:o OSCIN:x OSCOUT:x RESET:b RFSH:i D0:b D1:b D2:b D3:b D4:b D5:b D6:b D7:b '
              'DEN:o AEN:o ROMCS:o A0P:b A1P:b A2P:b A3P:b A4P:b A5P:b A6P:b A7P:b A8P:b A9:i A10:i A11:i A12:i A13:i A14:i A15:i '
              'KBD0:b KBD1:b KBD2:b KBD3:b KBD4:b RA7:o RA6:o RA5:o RA4:o RA3:o RA2:o RA1:o RA0:o WRD:o CAS:o RAS:o CHSEL:o '
              'NC55:n REFADJ:x GND:g VCC:v TAPEIN:i TVTAPE:o CONTROL:o RAMRM:o RAMCS:i WAIT:o RD:i HALT:i IORQ:i NMI:o'),
 'ROM2364': mk('A7:i A6:i A5:i A4:i A3:i A2:i A1:i A0:i D0:t D1:t D2:t GND:g D3:t D4:t D5:t D6:t D7:t '
               'A11:i A10:i CS:i A12:i A9:i A8:i VCC:v'),
 'LS244': mk('G1:i 1A1:i 2Y4:o 1A2:i 2Y3:o 1A3:i 2Y2:o 1A4:i 2Y1:o GND:g 2A1:i 1Y4:o 2A2:i 1Y3:o 2A3:i 1Y2:o 2A4:i 1Y1:o G2:i VCC:v'),
 'LS245': mk('DIR:i A1:b A2:b A3:b A4:b A5:b A6:b A7:b A8:b GND:g B8:b B7:b B6:b B5:b B4:b B3:b B2:b B1:b G:i VCC:v'),
 # 16K x 1 dynamic RAM; D (pin 2) and Q (pin 14) are on the same data line
 'DRAM4116': mk('VBB:x D:i WE:i RAS:i A0:i A2:i A1:i VDD:x VCC:v A5:i A4:i A3:i A6:i Q:t CAS:i VSS:g'),
 'DIODE': mk('A:x K:x'), 'RES': mk('P1:x P2:x'), 'CAP': mk('P1:x P2:x'), 'XTAL': mk('P1:x P2:x'),
 'CONN':  mk(' '.join('P%d:x' % i for i in range(1, 47))),
}
PWR = {
 'Z80': {11: '+5V', 29: 'GND'},
 'GA1500': {58: '+5V', 57: 'GND'},
 'ROM2364': {24: '+5V', 12: 'GND'},
 'LS244': {20: '+5V', 10: 'GND'},
 'LS245': {20: '+5V', 10: 'GND'},
 'DRAM4116': {9: '+5V', 16: 'GND'},
}
