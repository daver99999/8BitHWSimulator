"""Pinouts of the chips of the Timex Sinclair 1000 (US ZX81 Issue 1 reconstruction).
'name:dir' tokens in pin order. dir: i input, o output, t tri-state output,
b bidirectional, c open collector, n no-connect, g ground, v +5V, x unchecked.
"""
def mk(s):
    return [tuple(t.split(':')) for t in s.split()]

PINS = {
 'Z80': mk('A11:t A12:t A13:t A14:t A15:t CLK:i D4:b D3:b D5:b D6:b VCC:v D2:b D7:b D0:b D1:b '
           'INT:i NMI:i HALT:o MREQ:t IORQ:t RD:t WR:t BUSAK:o WAIT:i BUSRQ:i RESET:i M1:o RFSH:o '
           'GND:g A0:t A1:t A2:t A3:t A4:t A5:t A6:t A7:t A8:t A9:t A10:t'),
 # the ULA 2C210E (Ferranti), 40 pins: A0'-A8' are the alternate address lines (outputs during the refresh of a
 # video fetch, A0' and A1' inputs for the ports), the data pins are open collector drivers
 'ULA81': mk("A7P:b A8P:b A2P:b A1P:b A0P:b RD:i IORQ:i WR:i MREQ:i M1:i A14:i RAMCS:o ROMCS:o CLKB:o NMI:o "
             "TVTAPE:o HALT:i A15:i D7:b TAPEIN:i D6:b UKUS:i D5:b D4:b KBD4:i D3:b KBD3:i D2:b KBD2:i D1:b KBD1:i "
             "D0:b KBD0:i GND:g OSC:x A3P:b A4P:b A5P:b A6P:b VCC:v"),
 'ROM2364': mk('A7:i A6:i A5:i A4:i A3:i A2:i A1:i A0:i D0:t D1:t D2:t GND:g D3:t D4:t D5:t D6:t D7:t '
               'A11:i A10:i CS:i A12:i A9:i A8:i VCC:v'),
 # 2K x 8 static RAM in the 24 pin 6116 footprint (the board's RAM position takes a 4118 or a 6116)
 'RAM6116': mk('A7:i A6:i A5:i A4:i A3:i A2:i A1:i A0:i D0:b D1:b D2:b GND:g D3:b D4:b D5:b D6:b D7:b '
               'CS:i A10:i OE:i WE:i A9:i A8:i VCC:v'),
 'DIODE': mk('A:x K:x'), 'RES': mk('P1:x P2:x'), 'CAP': mk('P1:x P2:x'), 'XTAL': mk('P1:x P2:x'),
 'CONN':  mk(' '.join('P%d:x' % i for i in range(1, 47))),
 'Q3': mk('B:x C:x E:x'),
}
PWR = {
 'Z80': {11: '+5V', 29: 'GND'},
 'ULA81': {40: '+5V', 34: 'GND'},
 'ROM2364': {24: '+5V', 12: 'GND'},
 'RAM6116': {24: '+5V', 12: 'GND'},
}
