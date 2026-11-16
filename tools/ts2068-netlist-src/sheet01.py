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
N.pins('U16', {**addr16, **dat16, 20:'ROMCS', 27:'ROMOE', 22:'ROMOEP'})
N.note('U16 control pins: /CS 20 = ROMCS (SCLD 27); 22 and 27 (OE) INFERRED tied low/RD.')
a20 = {2:'A12',23:'A11',21:'A10',24:'A9',25:'A8',3:'A7',4:'A6',5:'A5',6:'A4',7:'A3',8:'A2',9:'A1',10:'A0'}
N.pins('U20', {**a20, **{11+i if i < 3 else 12+i: 'D%d' % i for i in range(8)}, 20:'EXROM', 22:'MREQ', 26:'RD'})
# ---- DRAM ----
for ref, cas, hi in (('U6','CASI',0),('U7','CASI',1),('U12','CAS2',0),('U13','CAS2',1),('U17','CAS3',0),('U18','CAS3',1)):
    N.chip(ref, '4416')
    d = ['D%dP' % (hi*4 + k) for k in range(4)]
    N.pins(ref, {1:'GND' if False else 'DROE', 2:d[0], 3:d[1], 15:d[2], 17:d[3], 4:'DRAMWE', 5:'RASI' if ref in ('U6','U7') else 'RAS',
                 16:cas, 14:'A0P',13:'A1P',12:'A2P',11:'A3P',8:'A4P',7:'A5P',6:'A6P',10:'A7P'})
N.note('4416 pins 2,3,15,17 are D1..D4 (data in AND out share 3-state); /OE tied to DROE (INFERRED low). Which RAS the upper 32K uses (RAS vs RASI) is INFERRED: check.')
# ---- AY-3-8912 U14 ----
N.chip('U14', 'AY8912')
N.pins('U14', {28:'D0',27:'D1',26:'D2',25:'D3',24:'D4',23:'D5',22:'D6',21:'D7', 19:'+5V', 17:'+5V', 20:'BC1', 18:'BDIR',
               16:'RESET', 15:'PHIC', 14:'IOA0',13:'IOA1',12:'IOA2',11:'IOA3',10:'IOA4',9:'IOA5',8:'IOA6',7:'IOA7'})
N.note('AY pin 17 on the schematic is labelled A8 and tied high; 19 is BC2 tied high. AY CLOCK = SCLD pin 19 (Table 2.1.8-1: clock C for the sound chip, 1.764 MHz).')
