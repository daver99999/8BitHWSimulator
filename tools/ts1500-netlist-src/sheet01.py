"""Timex Sinclair 1500: the circuit that is simulated, transcribed from the TS1500 schematic (a 1024 x 653 image).

LEGIBLE: the chips and their refs (U1 Z80, U2 8K ROM, U3 74LS244, U5 "COMPUTER LOGIC" gate array, U6 74LS245, U8-U15 4116
DRAMs, X1 6.5 MHz), the data/address buses, the signal names on every chip, the 1K resistors (R18 in the ROM's A8 line, R24
on /BUSRQ), the keyboard (40 keys, diodes D1-D8, KBD0-KBD4 and the half rows), the edge connector P1 names.

INFERRED (flagged below with INFERRED): the pin numbers of U5 (the 68 pin gate array): only 59 TAPE IN and 54 CH SEL OUTPUT are
legible. The others are counted along the symbol, see pinlib.py; verify them with a continuity test (docs/TS1500.md).
Also inferred: the direction of U6 (DIR on /RD), the diode to address line order, the keyboard connector pin numbers of
the half rows (read as 6 3 2 1 4 5 7 8 for A8..A15), and the pin order of the jacks and the modulator.

NOT transcribed (unreadable, not simulated): the video / tape / channel circuits (R7 100, R10 510, a 560 and a 270 ohm
resistor, D15, C6, the LED D16 option), the power section (L3, L4, D3, D4, D5, C24, C23's neighbours, the 5V regulator and
the -5V/+12V DC-DC converter for the DRAMs).  The DRAMs' VBB and VDD are tied to the -5V and +12V nets, which have no source here."""
from netlib import N
N.sheet_is('01')
# ---- Z80A U1 ----
N.chip('U1', 'Z80')
N.pins('U1', {1:'A11',2:'A12',3:'A13',4:'A14',5:'A15',6:'CPUCLK',7:'D4',8:'D3',9:'D5',10:'D6',12:'D2',13:'D7',14:'D0',15:'D1',
              16:'A6',17:'NMI',18:'HALT',19:'MREQ',20:'IORQ',21:'RD',22:'WR',23:'BUSAK',24:'WAIT',25:'BUSRQ',26:'RESET',27:'M1',28:'RFSH',
              30:'A0',31:'A1',32:'A2',33:'A3',34:'A4',35:'A5',36:'A6',37:'A7',38:'A8',39:'A9',40:'A10'})
N.note('/INT is wired to A6, as on the ZX81 (the edge connector has an INT contact: it is the A6 net).')
N.chip('R24', 'RES'); N.pins('R24', {1: '+5V', 2: 'BUSRQ'})        # 1K
# ---- U5, the gate array: PIN NUMBERS INFERRED except 59 (TAPE IN) and 54 (CH SEL OUTPUT) ----
N.chip('U5', 'GA1500')
N.pins('U5', {1:'MREQ',2:'WR',3:'M1',4:'CLKN',5:'CLK',6:'CPUCLK',7:'OSCIN',8:'OSCOUT',9:'RESET',10:'RFSH',
              11:'D0',12:'D1',13:'D2',14:'D3',15:'D4',16:'D5',17:'D6',18:'D7',19:'DEN',20:'AEN',21:'ROMCS',
              22:'A0P',23:'A1P',24:'A2P',25:'A3P',26:'A4P',27:'A5P',28:'A6P',29:'A7P',30:'A8P',
              31:'A9',32:'A10',33:'A11',34:'A12',35:'A13',36:'A14',37:'A15',
              38:'KBD0',39:'KBD1',40:'KBD2',41:'KBD3',42:'KBD4',
              43:'RA7',44:'RA6',45:'RA5',46:'RA4',47:'RA3',48:'RA2',49:'RA1',50:'RA0',51:'WRD',52:'CAS',53:'RAS',54:'CHSEL',
              59:'TAPEIN',60:'TVTAPE',61:'CONTROL',62:'RAMRM',63:'RAMCS',64:'WAIT',65:'RD',66:'HALT',67:'IORQ',68:'NMI'})
N.note('INFERRED: every pin number of U5 except 59 and 54. 55 and 56 (REF ADJ) are not connected here; 57 GND and 58 +5V are the only supply pins found.')
N.chip('X1', 'XTAL'); N.pins('X1', {1: 'OSCIN', 2: 'OSCOUT'})      # 6.5 MHz
N.chip('C23', 'CAP'); N.pins('C23', {1: 'RESET', 2: 'GND'})        # 22uF, the power-on reset RC
# ---- ROM U2 (8K): A0-A7 from U3, A8 through R18 (1K) and U5's A8', A9-A12 straight from the bus ----
N.chip('U2', 'ROM2364')
N.pins('U2', {8:'A0P',7:'A1P',6:'A2P',5:'A3P',4:'A4P',3:'A5P',2:'A6P',1:'A7P',23:'A8P',22:'A9',19:'A10',18:'A11',21:'A12',
              9:'D0P',10:'D1P',11:'D2P',13:'D3P',14:'D4P',15:'D5P',16:'D6P',17:'D7P',20:'ROMCS'})
N.chip('R18', 'RES'); N.pins('R18', {1: 'A8', 2: 'A8P'})            # 1K
# ---- U3 74LS244: CPU A0-A7 to the ROM side A0'-A7', enabled by /AEN (pins 1 and 19) ----
N.chip('U3', 'LS244')
N.pins('U3', {1:'AEN',19:'AEN', 11:'A0',9:'A0P', 8:'A1',12:'A1P', 13:'A2',7:'A2P', 6:'A3',14:'A3P',
              15:'A4',5:'A4P', 4:'A5',16:'A5P', 17:'A6',3:'A6P', 2:'A7',18:'A7P'})
# ---- U6 74LS245: CPU data bus <-> memory data bus D0'-D7', /G = /DEN, DIR = /RD (INFERRED) ----
N.chip('U6', 'LS245')
N.pins('U6', {1:'RD', 19:'DEN', **{2 + i: 'D%d' % i for i in range(8)}, **{18 - i: 'D%dP' % i for i in range(8)}})
N.note('INFERRED: U6 DIR on /RD.')
# ---- the eight DRAMs: one data bit each (U10 D0 ... U15 D7), D and Q on D0'..D7' ----
for ref, i in (('U10', 0), ('U9', 1), ('U8', 2), ('U11', 3), ('U12', 4), ('U13', 5), ('U14', 6), ('U15', 7)):
    N.chip(ref, 'DRAM4116')
    N.pins(ref, {1: '-5V', 2: 'D%dP' % i, 3: 'WRD', 4: 'RAS', 5: 'RA0', 6: 'RA2', 7: 'RA1', 8: '+12V', 10: 'RA5', 11: 'RA4',
                 12: 'RA3', 13: 'RA6', 14: 'D%dP' % i, 15: 'CAS'})
# ---- keyboard: KB1 (5 way, KBD0-4), KB2 (8 way, the half rows through D1-D8 from A8-A15) ----
N.chip('KB1', 'CONN'); N.pins('KB1', {1: 'KBD0', 2: 'KBD1', 3: 'KBD2', 4: 'KBD3', 5: 'KBD4'})
N.chip('KB2', 'CONN')
ROWPIN = (6, 3, 2, 1, 4, 5, 7, 8)                                   # the half rows A8..A15 (read from the schematic, low resolution)
N.pins('KB2', {ROWPIN[i]: 'ROW%d' % i for i in range(8)})
for d, i in (('D6', 0), ('D4', 1), ('D2', 2), ('D1', 3), ('D3', 4), ('D5', 5), ('D7', 6), ('D8', 7)):         # A8..A15
    N.chip(d, 'DIODE'); N.pins(d, {1: 'ROW%d' % i, 2: 'A%d' % (8 + i)})
N.note('INFERRED: which of D1-D8 is on which address line (the ZX81 order), and the KB2 pin numbers of the half rows.')
# ---- edge connector P1: pad 2k-1 is contact k on the component side (A), 2k on the underside (B). RAMRM replaces the TS1000s RAMCS ----
top = ['D7','RAMRM',None,'D0','D1','D2','D6','D5','D3','D4','A6','NMI','HALT','MREQ','IORQ','RD','WR','BUSAK','WAIT','BUSRQ','RESET','M1','RFSH']
bot = ['+5V','+9V',None,'GND','GND','CLK','A0','A1','A2','A3','A15','A14','A13','A12','A11','A10','A9','A8','A7','A6','A5','A4','ROMCS']
N.chip('P1', 'CONN')
N.pins('P1', {**{2 * k + 1: n for k, n in enumerate(top) if n}, **{2 * k + 2: n for k, n in enumerate(bot) if n}})
N.note('The edge connector is as drawn (data lines on the CPU bus, INT is the A6 net); the sequence is the TS1000s with RAMRM in place of RAMCS.')
# ---- video, tape, channel select: only the connections, nothing simulated ----
N.chip('D15', 'DIODE'); N.pins('D15', {1: 'TVTAPE', 2: 'VIDEO'})
N.chip('MOD', 'CONN'); N.pins('MOD', {1: 'VIDEO', 2: 'CHSEL', 3: 'GND', 4: '+5V'})
N.chip('MICJ', 'CONN'); N.pins('MICJ', {1: 'TVTAPE', 2: 'GND'})
N.chip('EARJ', 'CONN'); N.pins('EARJ', {1: 'TAPEIN', 2: 'GND'})
N.chip('DC', 'CONN'); N.pins('DC', {1: '+9V', 2: 'GND'})
N.note('INFERRED: the pin order of the jacks and the modulator, and that MIC and EAR are wired straight to U5 (the 100, 510, 560 and 270 ohm resistors of that corner are not transcribed).')
for c in ('C32', 'C33', 'C34'):                                     # .047 decoupling seen at the ROM, U3, U5
    N.chip(c, 'CAP'); N.pins(c, {1: '+5V', 2: 'GND'})
