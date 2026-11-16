"""Timex Sinclair 1000, board Issue 3 (US NTSC): the whole circuit that is simulated.
RECONSTRUCTED from published descriptions of the ZX81 circuit, NOT transcribed from a schematic of this board:
 - ULA pin-out and what it does: Andrew Rea's ULA replacement notes (oldcomputer.info), the ZX81 Issue 1 layout drawing
 - /INT is wired to A6 (Tynemouth Software, Tom Harte's CLK wiki); TR1 makes /WAIT from /HALT and /NMI (Wilf Rigter)
 - buffer resistors: 470 ohm on the ROM and RAM data lines, 1K from the CPU to the ROM address lines A0-A8 (Sinclair service
   articles, Rea): the ULA drives the primed lines (A0'-A8', D0'-D7') and wins
 - Issue 3: IC1 ULA, IC2 ROM, IC3 Z80A, IC4 RAM (2K x 8, 24 pins); R35-R42 data bus pull-ups, R43-R47 keyboard pull-ups
Everything marked INFERRED is a guess to be checked on the board: resistor numbers other than those two groups and R17, the
order of the keyboard connector pins, the RAM chip type, the crystal pins, and the US machine's UK/US strap."""
from netlib import N
N.sheet_is('01')
# ---- Z80A IC3 ----
N.chip('IC3', 'Z80')
N.pins('IC3', {1:'A11',2:'A12',3:'A13',4:'A14',5:'A15',6:'CPUCLK',7:'D4',8:'D3',9:'D5',10:'D6',12:'D2',13:'D7',14:'D0',15:'D1',
               16:'A6',17:'NMI',18:'HALT',19:'MREQ',20:'IORQ',21:'RD',22:'WR',23:'BUSAK',24:'WAIT',25:'BUSRQ',26:'RESET',27:'M1',28:'RFSH',
               30:'A0',31:'A1',32:'A2',33:'A3',34:'A4',35:'A5',36:'A6',37:'A7',38:'A8',39:'A9',40:'A10'})
N.note('/INT (IC3 pin 16) is wired to A6 (pin 36).')
for ref, net in (('R17', 'WAIT'), ('R60', 'BUSRQ'), ('R61', 'RESET')):
    N.chip(ref, 'RES'); N.pins(ref, {1: '+5V', 2: net})
N.note('INFERRED: R17 pulls /WAIT up (Tynemouth); R60 (/BUSRQ) and R61 (/RESET) are guesses. The RESET net is held low at power on by the machine (the real board has an RC).')
# ---- ULA IC1 ----
N.chip('IC1', 'ULA81')
N.pins('IC1', {1:'A7P',2:'A8P',3:'A2P',4:'A1P',5:'A0P',6:'RD',7:'IORQ',8:'WR',9:'MREQ',10:'M1',11:'A14',12:'RAMCS_U',13:'ROMCS_U',14:'CLKB',
               15:'NMI',16:'TVTAPE',17:'HALT',18:'A15',19:'D7',20:'TAPEIN',21:'D6',22:'UKUS',23:'D5',24:'D4',25:'KBD4',26:'D3',27:'KBD3',28:'D2',
               29:'KBD2',30:'D1',31:'KBD1',32:'D0',33:'KBD0',35:'OSC',36:'A3P',37:'A4P',38:'A5P',39:'A6P'})
# ---- clock: ULA /CLK (pin 14) -> TR2 (inverts) -> CPU clock ----
N.chip('TR2', 'Q3'); N.chip('R21', 'RES'); N.chip('R20', 'RES')
N.pins('R21', {1: 'CLKB', 2: 'TR2_B'}); N.pins('TR2', {1: 'TR2_B', 2: 'CPUCLK', 3: 'GND'}); N.pins('R20', {1: '+5V', 2: 'CPUCLK'})
N.note('INFERRED: R21 (base resistor) and R20 (collector pull-up) values and numbers. X1 (6.5 MHz ceramic resonator) between ULA pin 35 and GND.')
N.chip('X1', 'XTAL'); N.pins('X1', {1: 'OSC', 2: 'GND'})
# ---- /WAIT: TR1 pulls it low when /HALT is high and /NMI is low ----
N.chip('TR1', 'Q3'); N.chip('R22', 'RES')
N.pins('R22', {1: 'HALT', 2: 'TR1_B'}); N.pins('TR1', {1: 'TR1_B', 2: 'WAIT', 3: 'NMI'})
# ---- ROM IC2 (2364, 8K) ----
N.chip('IC2', 'ROM2364')
N.pins('IC2', {8:'A0P',7:'A1P',6:'A2P',5:'A3P',4:'A4P',3:'A5P',2:'A6P',1:'A7P',23:'A8P',22:'A9',19:'A10',18:'A11',21:'A12',
               9:'D0P',10:'D1P',11:'D2P',13:'D3P',14:'D4P',15:'D5P',16:'D6P',17:'D7P',20:'ROMCS'})
# ---- RAM IC4 (2K x 8) ----
N.chip('IC4', 'RAM6116')
N.pins('IC4', {8:'A0',7:'A1',6:'A2',5:'A3',4:'A4',3:'A5',2:'A6',1:'A7',23:'A8',22:'A9',19:'A10',
               9:'D0P',10:'D1P',11:'D2P',13:'D3P',14:'D4P',15:'D5P',16:'D6P',17:'D7P',18:'RAMCS',20:'RD',21:'WR'})
# ---- the buffer resistors ----
for i in range(8):                                        # 470 ohm: the CPU/ULA data bus to the ROM/RAM/expansion side
    N.chip('R%d' % (1 + i), 'RES'); N.pins('R%d' % (1 + i), {1: 'D%d' % i, 2: 'D%dP' % i})
for i in range(9):                                        # 1K: the CPU address to the ROM's A0'-A8'
    N.chip('R%d' % (51 + i), 'RES'); N.pins('R%d' % (51 + i), {1: 'A%d' % i, 2: 'A%dP' % i})
N.chip('R18', 'RES'); N.pins('R18', {1: 'RAMCS_U', 2: 'RAMCS'})   # 680 ohm after the ULA's chip selects
N.chip('R19', 'RES'); N.pins('R19', {1: 'ROMCS_U', 2: 'ROMCS'})
for i in range(8):                                        # R35-R42: 10K pull-ups on the data bus
    N.chip('R%d' % (35 + i), 'RES'); N.pins('R%d' % (35 + i), {1: '+5V', 2: 'D%d' % i})
for i in range(5):                                        # R43-R47: 10K pull-ups on the keyboard columns
    N.chip('R%d' % (43 + i), 'RES'); N.pins('R%d' % (43 + i), {1: '+5V', 2: 'KBD%d' % i})
N.chip('R30', 'RES'); N.pins('R30', {1: 'UKUS', 2: 'GND'})
N.note('INFERRED: R30 ties the ULA UK/US pin low on this US machine (the manual says D9, R30-R32 are for USA/France models).')
# ---- keyboard: KB1 (5 way, the ULA columns), KB2 (8 way, the half rows through D1-D8 from A8-A15) ----
N.chip('KB1', 'CONN'); N.pins('KB1', {1:'KBD0',2:'KBD1',3:'KBD2',4:'KBD3',5:'KBD4'})
N.chip('KB2', 'CONN'); N.pins('KB2', {i + 1: 'ROW%d' % i for i in range(8)})
for i in range(8):
    N.chip('D%d' % (1 + i), 'DIODE'); N.pins('D%d' % (1 + i), {1: 'ROW%d' % i, 2: 'A%d' % (8 + i)})
N.note('INFERRED: the pin order of KB1 and KB2 (KBD0-4 and the half rows A8-A15 in order).')
# ---- edge connector J1: pad 2k-1 is contact k on the top (component) side, 2k on the bottom ----
top = ['D7P','RAMCS',None,'D0P','D1P','D2P','D6P','D5P','D3P','D4P','A6','NMI','HALT','MREQ','IORQ','RD','WR','BUSAK','WAIT','BUSRQ','RESET','M1','RFSH']
bot = ['+5V','+9V',None,'GND','GND','CPUCLK','A0','A1','A2','A3','A15','A14','A13','A12','A11','A10','A9','A8','A7','A6','A5','A4','ROMCS']
N.chip('J1', 'CONN')
N.pins('J1', {**{2 * k + 1: n for k, n in enumerate(top) if n}, **{2 * k + 2: n for k, n in enumerate(bot) if n}})
N.note('The edge connector pins are from the ZX81 expansion bus; /INT (11A) and A6 are one net. The data lines are on the ROM/RAM side of the 470 ohm resistors (INFERRED).')
# ---- jacks and modulator (not simulated, for the signals) ----
N.chip('MIC', 'CONN'); N.pins('MIC', {1: 'TVTAPE', 2: 'GND'})
N.chip('EAR', 'CONN'); N.pins('EAR', {1: 'TAPEIN', 2: 'GND'})
N.chip('MOD', 'CONN'); N.pins('MOD', {1: 'TVTAPE', 2: 'GND', 3: '+5V'})
N.chip('DC', 'CONN'); N.pins('DC', {1: '+9V', 2: 'GND'})
