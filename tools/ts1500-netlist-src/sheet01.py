"""Timex Sinclair 1500: the circuit that is simulated, from the TS-1500 schematic "Rev 1.0, redrawn 2015" (Dick Berry), checked
against continuity readings on a real board.

READ FROM THE SCHEMATIC (all legible): every pin number of U5 (the 68 pin gate array), the Z80 U7, U2 (8K ROM), U3 74LS244, U6 74LS245 (DIR on /WR), the DRAMs U8-U15, X1 6.5 MHz, the keyboard
(J5 columns KBD0-4, J6 half rows A8-A15 through D13 D17 D10 D16 D12 D14 D15 D11), the edge connector P1 (data lines on the
primed bus D0'-D7', RAMRM, SYS CLK) and the video / tape / EAR corner.

CHECKED ON A REAL BOARD (continuity): Z80 CLK (pin 6) is on U5 pin 58 (SYS Clk), not 57 as the schematic says; Z80 D0-D7 (pins 14,15,12,8,7,9,10,13) are on U5 25,29,33,37,41,47,20,17 as the schematic says; Z80 A8-A15 (pins 38,39,40,1,2,3,4,5) are on U5 48,49,22,26,30,34,38,42 as the schematic says; Z80 -> U5: /M1 2, /NMI 3, /IORQ 4, /RD 5, /WR 6, /MREQ 7, /HALT 8, /WAIT 9, /RESET 10,
/RFSH 16.  The schematic and the measurements agree on every one of these.

OPEN: where U5 pin 57 (/CPU Clock) goes (nothing on the Z80's CLK: that is 58); whether the edge connector's clock contact is on the same net as the Z80's CLK,
pins 14 (/RAMCS) and 21 (60/50 Hz).
NOT transcribed: the power supplies (linear and switching), the transistor's pin order, L/C values. The DRAMs' VBB and VDD are
tied to the -5V and +12V nets, which have no source here."""
from netlib import N
N.sheet_is('01')
# ---- Z80A U7 (the schematic's ref) ----
N.chip('U7', 'Z80')
N.pins('U7', {1:'A11',2:'A12',3:'A13',4:'A14',5:'A15',6:'SYSCLK',7:'D4',8:'D3',9:'D5',10:'D6',12:'D2',13:'D7',14:'D0',15:'D1',
              16:'A6',17:'NMI',18:'HALT',19:'MREQ',20:'IORQ',21:'RD',22:'WR',23:'BUSAK',24:'WAIT',25:'BUSRQ',26:'RESET',27:'M1',28:'RFSH',
              30:'A0',31:'A1',32:'A2',33:'A3',34:'A4',35:'A5',36:'A6',37:'A7',38:'A8',39:'A9',40:'A10'})
N.note('/INT is wired to A6, as on the ZX81 (the edge connector has an INT contact: it is the A6 net).')
N.chip('R24', 'RES'); N.pins('R24', {1: '+5V', 2: 'BUSRQ'})        # 1K: measured 0.998K across it on a real board; the schematic read 680
# ---- U5, the gate array (NCR F811592): the pin numbers are the schematic's ----
N.chip('U5', 'GA1500')
N.pins('U5', {1:'KBD0', 2:'M1', 3:'NMI', 4:'IORQ', 5:'RD', 6:'WR', 7:'MREQ', 8:'HALT', 9:'WAIT', 10:'RESET', 11:'RAMRM', 12:'DEN',
              13:'AEN', 15:'ROMCS', 16:'RFSH', 17:'D7', 18:'GND', 19:'A2P', 20:'D6', 22:'A10', 23:'RA2', 24:'A3P', 25:'D0', 26:'A11', 27:'RA3',
              28:'A4P', 29:'D1', 30:'A12', 31:'RA4', 32:'A5P', 33:'D2', 34:'A13', 35:'RA5', 36:'A6P', 37:'D3', 38:'A14', 39:'RA6',
              40:'A7P', 41:'D4', 42:'A15', 43:'RA7', 44:'RA0', 46:'A0P', 47:'D5', 48:'A8P', 49:'A9', 50:'RA1', 51:'A1P',
              53:'TVTAPE', 54:'CHSEL', 55:'OSCOUT', 56:'OSCIN', 57:'CPUCLK', 58:'SYSCLK', 59:'TAPEIN',
              62:'CAS', 63:'RAS', 64:'WRD', 65:'KBD4', 66:'KBD3', 67:'KBD2', 68:'KBD1'})
N.note('U5 pin 18 ("CONT? AT GND") is on GND (0.18 ohm to pin 60 on a real board); pins 14 (/RAMCS, not connected to the Z80 or the DRAMs on a real board) and 21 (60/50 Hz) are not connected here.')
N.chip('X1', 'XTAL'); N.pins('X1', {1: 'OSCIN', 2: 'OSCOUT'})      # 6.5 MHz
N.chip('C23', 'CAP'); N.pins('C23', {1: 'RESET', 2: 'GND'})        # the power-on reset RC (the board's C23)
# ---- ROM U2 (8K): A0-A7 from U3 (A0'-A7'), A8 through R28 (1K) from the bus and U5's A8, A9-A12 straight from the bus ----
N.chip('U2', 'ROM2364')
N.pins('U2', {8:'A0P',7:'A1P',6:'A2P',5:'A3P',4:'A4P',3:'A5P',2:'A6P',1:'A7P',23:'A8P',22:'A9',19:'A10',18:'A11',21:'A12',
              9:'D0P',10:'D1P',11:'D2P',13:'D3P',14:'D4P',15:'D5P',16:'D6P',17:'D7P',20:'ROMCSR'})
N.chip('R28', 'RES'); N.pins('R28', {1: 'A8', 2: 'A8P'})            # 1K
N.chip('R25', 'RES'); N.pins('R25', {1: 'ROMCS', 2: 'ROMCSR'})      # 680 in the /ROMCS line (U5 15 -> ROM 20)
# ---- U3 74LS244: CPU A0-A7 to the ROM side A0'-A7', enabled by /AEN (pins 1 and 19) ----
N.chip('U3', 'LS244')
N.pins('U3', {1:'AEN',19:'AEN', 11:'A0',9:'A0P', 8:'A1',12:'A1P', 13:'A2',7:'A2P', 6:'A3',14:'A3P',
              15:'A4',5:'A4P', 4:'A5',16:'A5P', 17:'A6',3:'A6P', 2:'A7',18:'A7P'})
# ---- U6 74LS245: B side = CPU data bus D0-D7, A side = memory data bus D0'-D7'; /G = /DEN, DIR = /WR (high: A -> B, memories to CPU) ----
N.chip('U6', 'LS245')
N.pins('U6', {1:'WR', 19:'DEN', **{2 + i: 'D%dP' % i for i in range(8)}, **{18 - i: 'D%d' % i for i in range(8)}})
# ---- the eight DRAMs: one data bit each (U10 D0 ... U15 D7), D and Q on D0'..D7' ----
for ref, i in (('U10', 0), ('U9', 1), ('U8', 2), ('U11', 3), ('U12', 4), ('U13', 5), ('U14', 6), ('U15', 7)):
    N.chip(ref, 'DRAM4116')
    N.pins(ref, {1: '-5V', 2: 'D%dP' % i, 3: 'WRD', 4: 'RAS', 5: 'RA0', 6: 'RA2', 7: 'RA1', 8: '+12V', 10: 'RA5', 11: 'RA4',
                 12: 'RA3', 13: 'RA6', 14: 'D%dP' % i, 15: 'CAS'})
# ---- keyboard: J5 (5 way, KBD0-4), J6 (8 way, the half rows A8-A15 through the diodes D13 D17 D10 D16 D12 D14 D15 D11) ----
N.chip('J5', 'CONN'); N.pins('J5', {1: 'KBD0', 2: 'KBD1', 3: 'KBD2', 4: 'KBD3', 5: 'KBD4'})
N.chip('J6', 'CONN')
N.pins('J6', {i + 1: 'ROW%d' % i for i in range(8)})
for d, i in (('D13', 0), ('D17', 1), ('D10', 2), ('D16', 3), ('D12', 4), ('D14', 5), ('D15', 6), ('D11', 7)):         # A8..A15
    N.chip(d, 'DIODE'); N.pins(d, {1: 'ROW%d' % i, 2: 'A%d' % (8 + i)})
# ---- edge connector P1: pad 2k-1 is contact k on the component side (A), 2k on the underside (B). The data lines are the primed bus ----
top = ['D7P','RAMRM',None,'D0P','D1P','D2P','D6P','D5P','D3P','D4P','A6','NMI','HALT','MREQ','IORQ','RD','WR','BUSAK','WAIT','BUSRQ','RESET','M1','RFSH']
bot = ['+5V','+9V',None,'GND','GND','SYSCLK','A0','A1','A2','A3','A15','A14','A13','A12','A11','A10','A9','A8','A7','A6','A5','A4','ROMCS']
N.chip('P1', 'CONN')
N.pins('P1', {**{2 * k + 1: n for k, n in enumerate(top) if n}, **{2 * k + 2: n for k, n in enumerate(bot) if n}})
N.note('P1: /INT (contact A11) is the A6 net (the Z80 /INT pin is wired to A6); the data contacts are on the primed bus D0\'-D7\' (memory side of U6).')
# ---- video: U5 TV-TAPE -> D19 -> R1 560 -> modulator video in (R2 270 to GND); channel select from U5 pin 54 ----
N.chip('D19', 'DIODE'); N.pins('D19', {1: 'TVTAPE', 2: 'DK'})
N.chip('R1', 'RES'); N.pins('R1', {1: 'DK', 2: 'VIDEO'})
N.chip('R2', 'RES'); N.pins('R2', {1: 'VIDEO', 2: 'GND'})
N.chip('MOD', 'CONN'); N.pins('MOD', {1: 'VIDEO', 2: 'CHSEL', 3: 'GND', 4: '+5V'})
# ---- tape out: TV-TAPE -> R20 510K -> C19 47pF -> MIC tip; R19 510K + C18 47nF across the jack, R17 100 to GND ----
N.chip('R20', 'RES'); N.pins('R20', {1: 'TVTAPE', 2: 'MICA'})
N.chip('C19', 'CAP'); N.pins('C19', {1: 'MICA', 2: 'MICT'})
N.chip('R19', 'RES'); N.pins('R19', {1: 'MICT', 2: 'MICS'})
N.chip('C18', 'CAP'); N.pins('C18', {1: 'MICT', 2: 'MICS'})
N.chip('R17', 'RES'); N.pins('R17', {1: 'MICS', 2: 'GND'})
N.chip('J2', 'CONN'); N.pins('J2', {1: 'MICT', 2: 'MICS'})
# ---- tape in: EAR jack, C13 47nF and R16 220 to GND, a 47pF (also C19 on the schematic) and R22 4.7K to Q1's base (R32 220K / R21 18K bias),
#      Q1 MPS2359, R31 220 in the emitter, R18 18K the collector load to +5V, the collector is TAPE-IN (U5 59) ----
N.chip('J1', 'CONN'); N.pins('J1', {1: 'EARIN', 2: 'GND'})
N.chip('C13', 'CAP'); N.pins('C13', {1: 'EARIN', 2: 'GND'})
N.chip('R16', 'RES'); N.pins('R16', {1: 'EARIN', 2: 'GND'})
N.chip('C19E', 'CAP'); N.pins('C19E', {1: 'EARIN', 2: 'EARB'})
N.chip('R22', 'RES'); N.pins('R22', {1: 'EARB', 2: 'BASE'})
N.chip('R32', 'RES'); N.pins('R32', {1: 'BASE', 2: '+5V'})
N.chip('R21', 'RES'); N.pins('R21', {1: 'BASE', 2: 'GND'})
N.chip('Q1', 'Q3'); N.pins('Q1', {1: 'BASE', 2: 'TAPEIN', 3: 'EMIT'})
N.chip('R31', 'RES'); N.pins('R31', {1: 'EMIT', 2: 'GND'})
N.chip('R18', 'RES'); N.pins('R18', {1: 'TAPEIN', 2: '+5V'})
N.note('INFERRED: the pin order of Q1 (base, collector, emitter as 1, 2, 3) and the jacks.')
N.chip('J3', 'CONN'); N.pins('J3', {1: '+9V', 2: 'GND'})
for c in ('C32', 'C33', 'C34'):                                     # decoupling (the schematic lists the filter capacitors by rail)
    N.chip(c, 'CAP'); N.pins(c, {1: '+5V', 2: 'GND'})
