"""Timex Sinclair 1000 (ZX81, USA), board Issue 3: the whole circuit that is simulated.
Transcribed from the schematic "SINCLAIR ZX81, DRW No. SRC 049, re-drawn by Ron Reuter" (www.mainbyte.com/ts1000), which has
the USA-only parts (R30-R32, D9, the channel select switch) and the RP1/RP3 resistor packs that the Issue 3 board has as
R35-R42 and R43-R47. The schematic is of the UK/USA Issue 1 circuit; the Issue 3 board has the same circuit and reference
numbers (the board's silkscreen was checked for R17, R27, R31, R32, R34, R43-R47, D9, IC1-IC4).
Not on the schematic, so INFERRED: which of R35-R42 is on which data line and R43-R47 on which keyboard line, the pin order of
KB2 (the half rows A8..A15 in order) and KB1 (KBD0..KBD4), the pin order of the jacks and the modulator, and the RAM chip: the
schematic's IC4 is a 4118 (1Kx8); the TS1000 has 2K, a 6116 type in the same footprint with A10 on pin 19 (link L2)."""
from netlib import N
N.sheet_is('01')
# ---- Z80A IC3 ----
N.chip('IC3', 'Z80')
N.pins('IC3', {1:'A11',2:'A12',3:'A13',4:'A14',5:'A15',6:'CPUCLK',7:'D4',8:'D3',9:'D5',10:'D6',12:'D2',13:'D7',14:'D0',15:'D1',
               16:'A6',17:'NMI',18:'HALT',19:'MREQ',20:'IORQ',21:'RD',22:'WR',23:'BUSAK',24:'WAIT',25:'BUSRQ',26:'RESET',27:'M1',28:'RFSH',
               30:'A0',31:'A1',32:'A2',33:'A3',34:'A4',35:'A5',36:'A6',37:'A7',38:'A8',39:'A9',40:'A10'})
N.note('/INT (IC3 pin 16) is wired to A6 (pin 36), as drawn.')
N.chip('R17', 'RES'); N.pins('R17', {1: '+5V', 2: 'WAIT'})       # 1K
N.chip('R16', 'RES'); N.pins('R16', {1: '+5V', 2: 'BUSRQ'})      # 1K
N.chip('R15', 'RES'); N.pins('R15', {1: '+5V', 2: 'RESET'})      # 220K
N.chip('C5', 'CAP'); N.pins('C5', {1: 'RESET', 2: 'GND'})        # 1uF: the power-on reset RC (the machine drives /RESET low for 50 ms)
# ---- ULA IC1 ----
N.chip('IC1', 'ULA81')
N.pins('IC1', {1:'A7P',2:'A8P',3:'A2P',4:'A1P',5:'A0P',6:'RD',7:'IORQ',8:'WR',9:'MREQ',10:'M1',11:'A14',12:'RAMCS_U',13:'ROMCS_U',14:'CLKB',
               15:'NMI',16:'TVTAPE',17:'HALT',18:'A15',19:'D7',20:'TAPEIN',21:'D6',22:'UKUS',23:'D5',24:'D4',25:'KBD4',26:'D3',27:'KBD3',28:'D2',
               29:'KBD2',30:'D1',31:'KBD1',32:'D0',33:'KBD0',35:'OSC',36:'A3P',37:'A4P',38:'A5P',39:'A6P'})
# ---- CPU clock: ULA pin 14 -> R6 (2K2, C7 47pF across it) -> TR2 base; TR2 inverts it, R5 330R pull-up ----
N.chip('TR2', 'Q3'); N.chip('R6', 'RES'); N.chip('C7', 'CAP'); N.chip('R5', 'RES')
N.pins('R6', {1: 'CLKB', 2: 'TR2_B'}); N.pins('C7', {1: 'CLKB', 2: 'TR2_B'})
N.pins('TR2', {1: 'TR2_B', 2: 'CPUCLK', 3: 'GND'}); N.pins('R5', {1: '+5V', 2: 'CPUCLK'})
# ---- oscillator: ULA pin 35, R4 18K to 0V, C6 100pF in series with the 6.5 MHz ceramic filter X1 to 0V ----
N.chip('R4', 'RES'); N.pins('R4', {1: 'OSC', 2: 'GND'})
N.chip('C6', 'CAP'); N.pins('C6', {1: 'OSC', 2: 'X1T'})
N.chip('X1', 'XTAL'); N.pins('X1', {1: 'X1T', 2: 'GND'})
# ---- /WAIT: TR1 pulls it low when /HALT is high and /NMI is low; R1 10K (C1 47pF across it) from /HALT to the base ----
N.chip('TR1', 'Q3'); N.chip('R1', 'RES'); N.chip('C1', 'CAP')
N.pins('R1', {1: 'HALT', 2: 'TR1_B'}); N.pins('C1', {1: 'HALT', 2: 'TR1_B'}); N.pins('TR1', {1: 'TR1_B', 2: 'WAIT', 3: 'NMI'})
# ---- ROM IC2 (2364, 8K) ----
N.chip('IC2', 'ROM2364')
N.pins('IC2', {8:'A0P',7:'A1P',6:'A2P',5:'A3P',4:'A4P',3:'A5P',2:'A6P',1:'A7P',23:'A8P',22:'A9',19:'A10',18:'A11',21:'A12',
               9:'D0P',10:'D1P',11:'D2P',13:'D3P',14:'D4P',15:'D5P',16:'D6P',17:'D7P',20:'ROMCS'})
# ---- RAM IC4 (2K x 8): pins 18 and 20 are both /RAMCS, 21 /WR, 19 A10 (link L2) ----
N.chip('IC4', 'RAM6116')
N.pins('IC4', {8:'A0',7:'A1',6:'A2',5:'A3',4:'A4',3:'A5',2:'A6',1:'A7',23:'A8',22:'A9',19:'A10',
               9:'D0P',10:'D1P',11:'D2P',13:'D3P',14:'D4P',15:'D5P',16:'D6P',17:'D7P',18:'RAMCS',20:'RAMCS',21:'WR'})
# ---- the buffer resistors ----
for r, i in (('R14', 0), ('R10', 1), ('R9', 2), ('R8', 3), ('R7', 4), ('R11', 5), ('R12', 6), ('R13', 7)):     # 470 ohm, data bus
    N.chip(r, 'RES'); N.pins(r, {1: 'D%d' % i, 2: 'D%dP' % i})
for r, i in (('R26', 0), ('R18', 1), ('R19', 2), ('R25', 3), ('R24', 4), ('R23', 5), ('R20', 6), ('R21', 7), ('R22', 8)):  # 1K, ROM address
    N.chip(r, 'RES'); N.pins(r, {1: 'A%d' % i, 2: 'A%dP' % i})
N.chip('R2', 'RES'); N.pins('R2', {1: 'RAMCS_U', 2: 'RAMCS'})    # 680 ohm after the ULA's chip selects
N.chip('R28', 'RES'); N.pins('R28', {1: 'ROMCS_U', 2: 'ROMCS'})  # 680 ohm
# R35-R42 (RP1 on the schematic): 10K pull-ups on the data bus. MEASURED on a real Issue 3 (continuity to the Z80's data pins) for
# R35-R39 and R42; R40 and R41 are D1 and D3 in an order not read from the silkscreen (the pair is electrically the same).
R_DLINE = {35: 6, 36: 5, 37: 4, 38: 7, 39: 0, 40: 1, 41: 3, 42: 2}
for r, d in R_DLINE.items():
    N.chip('R%d' % r, 'RES'); N.pins('R%d' % r, {1: '+5V', 2: 'D%d' % d})
for i in range(5):                                        # R43-R47 (RP3): 10K pull-ups on the keyboard columns
    N.chip('R%d' % (43 + i), 'RES'); N.pins('R%d' % (43 + i), {1: '+5V', 2: 'KBD%d' % i})
N.note('INFERRED: which of R43-R47 on which KBD line (the resistor packs are drawn in line order).')
N.chip('R30', 'RES'); N.pins('R30', {1: 'UKUS', 2: 'GND'})       # 10 ohm, USA only: the UK/USA pin is low on this machine
# ---- keyboard: KB1 (5 way, the ULA columns), KB2 (8 way, the half rows through D1-D8 from A8-A15) ----
N.chip('KB1', 'CONN'); N.pins('KB1', {1:'KBD0',2:'KBD1',3:'KBD2',4:'KBD3',5:'KBD4'})
# KB2 pin n is on diode Dn (MEASURED on a real Issue 3: pin 1 to the diode at the left, the rest in order). The diodes are on the
# address lines A11 A10 A12 A9 A13 A8 A14 A15 (D1..D8), so the half rows (ROW0 = A8 .. ROW7 = A15) are on KB2 pins 6 4 2 1 3 5 7 8.
N.chip('KB2', 'CONN'); N.pins('KB2', {i + 1: 'ROW%d' % r for i, r in enumerate((3, 2, 4, 1, 5, 0, 6, 7))})
for d, i in (('D6', 0), ('D4', 1), ('D2', 2), ('D1', 3), ('D3', 4), ('D5', 5), ('D7', 6), ('D8', 7)):         # A8..A15
    N.chip(d, 'DIODE'); N.pins(d, {1: 'ROW%d' % i, 2: 'A%d' % (8 + i)})
N.note('KB1 (R43-R47, KBD0-4) and the KB2 pin order were checked on a real Issue 3; which address line each diode D1-D8 is on is still from the schematic.')
# ---- edge connector J1: pad 2k-1 is contact k on the component side (A), 2k on the underside (B) ----
top = ['D7P','RAMCS',None,'D0P','D1P','D2P','D6P','D5P','D3P','D4P','A6','NMI','HALT','MREQ','IORQ','RD','WR','BUSAK','WAIT','BUSRQ','RESET','M1','RFSH']
bot = ['+5V','+9V',None,'GND','GND','CPUCLK','A0','A1','A2','A3','A15','A14','A13','A12','A11','A10','A9','A8','A7','A6','A5','A4','ROMCS']
N.chip('J1', 'CONN')
N.pins('J1', {**{2 * k + 1: n for k, n in enumerate(top) if n}, **{2 * k + 2: n for k, n in enumerate(bot) if n}})
N.note('The edge connector is as drawn (the data lines are the primed ones, on the ROM/RAM side of the 470 ohm resistors; INT is the A6 net).')
# ---- video, tape and the modulator (not simulated, but wired) ----
N.chip('D9', 'DIODE'); N.pins('D9', {1: 'TVTAPE', 2: 'D9K'})
N.chip('R32', 'RES'); N.pins('R32', {1: 'D9K', 2: 'USA3'})       # 560 ohm, USA only
N.chip('R31', 'RES'); N.pins('R31', {1: 'USA3', 2: 'GND'})       # 390 ohm, USA only
N.chip('MOD', 'CONN'); N.pins('MOD', {1: 'TVTAPE', 2: 'USA3', 3: 'GND', 4: '+5V'})
N.note('INFERRED: the pin order of the modulator (UK2, USA3, 0V, +5V); the real one has more pins (Fr1, Fr2, UK1, USA2) and the USA channel select switch.')
N.chip('R29', 'RES'); N.pins('R29', {1: 'TVTAPE', 2: 'MICA'})    # 1M
N.chip('C12', 'CAP'); N.pins('C12', {1: 'MICA', 2: 'MIC'})       # 47pF
N.chip('R27', 'RES'); N.pins('R27', {1: 'MIC', 2: 'GND'})        # 1K
N.chip('C11', 'CAP'); N.pins('C11', {1: 'MIC', 2: 'GND'})        # 47nF
N.chip('MICJ', 'CONN'); N.pins('MICJ', {1: 'MIC', 2: 'GND'})
N.chip('R34', 'RES'); N.pins('R34', {1: 'EAR', 2: 'GND'})        # 220 ohm
N.chip('C10', 'CAP'); N.pins('C10', {1: 'EAR', 2: 'EARC'})       # 10nF
N.chip('R33', 'RES'); N.pins('R33', {1: 'EARC', 2: 'TAPEIN'})    # 4K7
N.chip('EARJ', 'CONN'); N.pins('EARJ', {1: 'EAR', 2: 'GND'})
N.chip('DC', 'CONN'); N.pins('DC', {1: '+9V', 2: 'GND'})
N.chip('C3', 'CAP'); N.pins('C3', {1: '+9V', 2: 'GND'})          # 22uF at the 9V input
for c in ('C2', 'C4', 'C8', 'C9'):                               # 47nF decoupling on the 5V rail
    N.chip(c, 'CAP'); N.pins(c, {1: '+5V', 2: 'GND'})
N.note('The 7805 (REG) is not modeled (the +5V rail is ideal); this board has a switching regulator instead.')
