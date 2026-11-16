"""CPU, buses, ROMs, DRAM, sound: everything read so far from the schematic.
Items marked INFERRED were not traced wire by wire (the bus is drawn as unlabelled
parallel lines); they follow standard Z80/ROM/DRAM wiring and are checked by booting."""
from netlib import N
N.sheet_is('01')
# ---- Z80A U19 ----
N.chip('U19', 'Z80')
cpu = {i: 'A%d' % (10 + i) if False else None for i in ()}
z = {1:'A11',2:'A12',3:'A13',4:'A14',5:'A15',6:'CPUCLK',7:'D4',8:'D3',9:'D5',10:'D6',12:'D2',13:'D7',14:'D0',15:'D1',
     16:'INT',17:'NMI',19:'MREQ',20:'IORQ',21:'RD',22:'WR',24:'WAIT',25:'BUSRQ',26:'RESET',27:'M1',28:'RFSH',
     30:'A0',31:'A1',32:'A2',33:'A3',34:'A4',35:'A5',36:'A6',37:'A7',38:'A8',39:'A9',40:'A10'}
N.pins('U19', z)
for ref, val, net in (('R40','10K','NMI'),('R41','1.5K','WAIT'),('R42','1K','BUSRQ'),('R43','220K','RESET')):
    N.chip(ref, 'RES'); N.pins(ref, {1: '+5V', 2: net})
# ---- SCLD U3 (pins as drawn) ----
N.chip('U3', 'SCLD')
scld = {49:'KB0',50:'KB1',51:'KB2',52:'KB3',53:'KB4', 13:'OSC_OUT',14:'OSC_IN',15:'TAPE_IN',
        12:'BE',11:'EXROM',10:'PHI_N',32:'ROSCS',16:'SPKR_TAPE_OUT',19:'PHIC',18:'BDIR',17:'BC1',41:'CASI',
        37:'DRAMWE',34:'U',35:'V',33:'Y',36:'RASI',46:'BLUE',47:'GREEN',48:'RED',42:'CPUCLK',
        67:'INT',64:'IORQ',66:'MREQ',65:'WR',63:'RD',54:'RDDIR',68:'RFSH',38:'A7R',45:'A15',44:'A14',9:'A13',
        8:'A7',7:'A6',6:'A5',5:'A4',4:'A3',3:'A2',2:'A1',1:'A0',
        22:'MA7',30:'MA5',28:'MA6',24:'MA4',23:'MA3',25:'MA2',29:'MA1',31:'MA0',
        20:'TS',21:'MUX',27:'ROMCS',39:'CAS3',40:'CAS2',43:'+5V',26:'GND',
        57:'D0P',55:'D1P',56:'D2P',58:'D3P',61:'D4P',59:'D5P',60:'D6P',62:'D7P'}
N.note('SCLD pin functions are from Technical Manual Table 2.1.8-1: 54 = RD* (read direction output to U9 DIR), 63 = RD (input from the CPU), 38 = ATR (refresh and the 8th row address bit of the 32K), 19 = clock C for the AY (1.764 MHz), 10 = buffered CPU clock to the rear connector, 20 = TS (tri-state control of the address and display data buffers).')
N.pins('U3', scld)
# ---- 74LS245 U9: CPU data bus <-> primed (RAM side) data bus ----
N.chip('U9', 'LS245')
N.pins('U9', {1:'RDDIR', 19:'TS', **{2+i: 'D%d' % i for i in range(8)}, **{18-i: 'D%dP' % i for i in range(8)}})
# ---- 74LS245 U15: CPU control to the dock ----
N.chip('U15', 'LS245')
N.pins('U15', {1:'+5V', 19:'GND', 2:'RFSH',3:'MREQ',4:'IORQ',5:'WR',6:'RD',7:'A13',8:'A14',9:'A15',
               18:'RFSHB',17:'MREQB',16:'IORQB',15:'WRB',14:'RDB',13:'A13B',12:'A14B',11:'A15B'})
N.note('U15 DIR (pin 1) and E (pin 19) as drawn: DIR high (A to B), E low = enabled. Pin 10 GND.')
# ---- ROMs ----
N.chip('U16', 'ROM23128'); N.chip('U20', 'ROM2364')
addr16 = {2:'A12',3:'A7',4:'A6',5:'A5',6:'A4',7:'A3',8:'A2',9:'A1',10:'A0',21:'A10',23:'A11',24:'A9',25:'A8',26:'A13'}
dat16 = {11:'D0',12:'D1',13:'D2',15:'D3',16:'D4',17:'D5',18:'D6',19:'D7'}
N.pins('U16', {**addr16, **dat16, 20:'ROMCS', 27:'+5V', 22:'RD'})
N.note('U16: /CS 20 = ROMCS (SCLD 27); pin 22 (OE) = RD and pin 27 = +5V through R70 10K are INFERRED from the drawing.')
a20 = {2:'A12',23:'A11',21:'A10',24:'A9',25:'A8',3:'A7',4:'A6',5:'A5',6:'A4',7:'A3',8:'A2',9:'A1',10:'A0'}
N.pins('U20', {**a20, **{11+i if i < 3 else 12+i: 'D%d' % i for i in range(8)}, 20:'EXROM', 22:'MREQ', 26:'RD'})
# ---- DRAM ----
# U6/U7: the 16K display RAM, on the SCLD's own strobes (RASI, CASI, DRAMWE), addressed by MA0-MA7 and on the primed data bus D0'-D7'
# (Technical Manual 2.1.10: two 4416s).  U12/U13 (chunk 4-5, CAS2) and U17/U18 (CAS3): the 32K upper RAM, on the CPU data bus,
# /WR straight from the CPU (schematic label "WR" on their pin 4), /RAS from /MREQ (INFERRED: RAS-only refresh by the Z80), addresses A0'-A7'.
for ref, hi in (('U6',0),('U7',1)):
    N.chip(ref, '4416')
    N.pins(ref, {1:'GND', 2:'D%dP' % (hi*4), 3:'D%dP' % (hi*4+1), 15:'D%dP' % (hi*4+2), 17:'D%dP' % (hi*4+3),
                 4:'DRAMWE', 5:'RASI', 16:'CASI', 14:'MA0',13:'MA1',12:'MA2',11:'MA3',8:'MA4',7:'MA5',6:'MA6',10:'MA7'})
for ref, cas, hi in (('U12','CAS2',0),('U13','CAS2',1),('U17','CAS3',0),('U18','CAS3',1)):
    N.chip(ref, '4416')
    N.pins(ref, {1:'GND', 2:'D%d' % (hi*4), 3:'D%d' % (hi*4+1), 15:'D%d' % (hi*4+2), 17:'D%d' % (hi*4+3),
                 4:'WR', 5:'MREQ', 16:cas, 14:'A0P',13:'A1P',12:'A2P',11:'A3P',8:'A4P',7:'A5P',6:'A6P',10:'A7P'})
N.note('INFERRED: /RAS of the upper RAM is the CPU /MREQ (the schematic shows the wire but its source was not traced); the 4416 /OE pins are tied to GND (they are joined to a node on the schematic).')
# ---- AY-3-8912 U14 ----
N.chip('U14', 'AY8912')
N.pins('U14', {2:'GND', 28:'D0',27:'D1',26:'D2',25:'D3',24:'D4',23:'D5',22:'D6',21:'D7', 19:'+5V', 17:'+5V', 20:'BC1', 18:'BDIR',
               16:'RESET', 15:'PHIC', 14:'IOA0',13:'IOA1',12:'IOA2',11:'IOA3',10:'IOA4',9:'IOA5',8:'IOA6',7:'IOA7'})
N.note('AY pin 17 on the schematic is labelled A8 and tied high; 19 is BC2 tied high. AY CLOCK = SCLD pin 19 (Table 2.1.8-1: clock C for the sound chip, 1.764 MHz).')
