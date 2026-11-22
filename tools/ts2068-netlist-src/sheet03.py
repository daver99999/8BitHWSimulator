"""Keyboard connector J9, the diodes of the keyboard and joysticks, the joystick connectors J5/J6, the cartridge dock J4,
crystal Y1.  From the schematic (sheet right half) and Technical Manual 2.4.2, 2.4.4, 2.4.7.
NOT in the netlist yet (analog, not simulated): the power supply (U1, U8, Q1), the video and RF circuits (U4, Q2-Q5, coils),
the tape and sound output stages, the rear connector P1."""
from netlib import N
N.sheet_is('03')
# ---- crystal ----
N.chip('Y1', 'XTAL'); N.pins('Y1', {1: 'OSC_OUT', 2: 'OSC_IN'})
# ---- keyboard: J9 flex (Technical Manual 2.4.7; its pins are numbered 0-13 there, 1-14 here) and the eight diodes ----
N.chip('J9', 'CONN')
N.pins('J9', {1:'GND', 2:'KB0', 3:'KB1', 4:'KB2', 5:'KB3', 6:'KB4',
              7:'ROW_A11', 8:'ROW_A10', 9:'ROW_A9', 10:'ROW_A12', 11:'ROW_A13', 12:'ROW_A8', 13:'ROW_A14', 14:'ROW_A15'})
# diode: pin 1 anode (the key row), pin 2 cathode (the address line)
for ref, row, addr in (('CR6','ROW_A11','A11'),('CR7','ROW_A10','A10'),('CR8','ROW_A9','A9'),('CR9','ROW_A12','A12'),
                       ('CR10','ROW_A13','A13B'),('CR11','ROW_A8','A8'),('CR12','ROW_A14','A14B'),('CR13','ROW_A15','A15B')):
    N.chip(ref, 'DIODE'); N.pins(ref, {1: row, 2: addr})
# ---- joysticks: J5 player 1 (strobe from A8), J6 player 2 (strobe from A9); AY port A lines through diodes ----
for conn, strobe, diodes, fire in (('J5', 'STROBE1', ('CR17','CR16','CR15','CR14'), 'CR18'),
                                   ('J6', 'STROBE2', ('CR22','CR21','CR20','CR19'), 'CR23')):
    N.chip(conn, 'CONN')
    N.pins(conn, {5: 'NC_' + conn, 7: '+5V', 8: strobe, 9: 'GND'})
    for i, cr in enumerate(diodes):                       # pins 1-4 = DIR1..DIR4 = IOA0..IOA3
        N.chip(cr, 'DIODE')
        N.pins(cr, {1: '%s_DIR%d' % (conn, i + 1), 2: 'IOA%d' % i})
        N.pins(conn, {i + 1: '%s_DIR%d' % (conn, i + 1)})
    N.chip(fire, 'DIODE'); N.pins(fire, {1: conn + '_FIRE', 2: 'IOA7'}); N.pins(conn, {6: conn + '_FIRE'})
# the strobe: a 2N3904 inverts A8 (A9): Q8 (Q7) collector to J5 (J6) pin 8, with a 20K base resistor R45 (R44)
for q, rb, addr, strobe in (('Q8', 'R45', 'A8', 'STROBE1'), ('Q7', 'R44', 'A9', 'STROBE2')):
    N.chip(q, 'Q3'); N.chip(rb, 'RES')
    N.pins(rb, {1: addr, 2: q + '_B'})
    N.pins(q, {1: q + '_B', 2: strobe, 3: 'GND'})
N.note('The strobe transistors are modelled as an inverter (base high -> strobe low); the schematic shows L5/L8 ferrite beads in the strobe lines, ignored.')
# ---- cartridge dock J4: 64 contacts, 32 positions with a B (solder) side and an A (component) side ----
# Pin list from the Timex Technical Manual, given by David Green. The simulator's pin number is 2n-1 for position nB and 2n for nA,
# so pins 1 and 2 are 1B and 1A, pins 3 and 4 are 2B and 2A, and so on up to 63 and 64 (32B, 32A); both ends of both sides are ground.
# Signals that are not in the netlist (the analog and video circuits, the power supply, the tape input) are left unconnected.
dockB = {1:'GND', 2:'SPKR_TAPE_OUT', 4:'+5V', 7:'GND', 8:'GND', 10:'A0', 11:'A1', 12:'A2', 13:'A3', 14:'A15B', 15:'A14B', 16:'A13B',
         17:'A12', 18:'A11', 19:'A10', 20:'A9', 21:'A8', 22:'A7', 23:'A6', 24:'A5', 25:'A4', 32:'GND'}
dockA = {1:'GND', 3:'A7P', 4:'D7', 7:'D0', 8:'D1', 9:'D2', 10:'D6', 11:'D5', 12:'D3', 13:'D4', 14:'INT', 15:'NMI', 16:'HALT',
         17:'MREQB', 18:'IORQB', 19:'RDB', 20:'WRB', 21:'BUSAK', 22:'WAIT', 23:'BUSRQ', 24:'RESET', 25:'M1', 26:'RFSHB',
         27:'EXROM', 28:'ROSCS', 29:'BE', 32:'GND'}
N.chip('J4', 'CONN')
N.pins('J4', {**{2 * n - 1: net for n, net in dockB.items()}, **{2 * n: net for n, net in dockA.items()}})
N.pins('U19', {18: 'HALT', 23: 'BUSAK'})        # the Z80's /HALT and /BUSAK go to the dock
N.note('J4 (64 contacts, 1B-32B and 1A-32A) is from the Technical Manual pin list. Not connected here: +15V (3B), tape EAR in (2A), R G B and VIDEO (27B-29B, 31B), SOUND (31A), DZIN and DZOUT, BUSISO (30B), IOA5 (30A), pin 9B ("0" in the list) and the two Slot and one Unused positions. A7RB (3A) is the muxed A7 of U11 (A7P).')
# pull-ups on the dock-facing active-low lines the SCLD only receives
N.chip('R75', 'RES'); N.pins('R75', {1: '+5V', 2: 'BE'})
N.note('R75: pull-up on /BE (INFERRED: the schematic has a diode and the P1/J4 BE wires there; reference R75 is in its list of numbers that do not exist).')
