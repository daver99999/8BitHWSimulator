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
# ---- cartridge dock J4 (2x18): pin numbers as on the schematic, 1-36 ----
N.chip('J4', 'CONN')
dock = {1:'A14B', 2:'+5V', 3:'A12', 4:'A13B', 5:'D0', 6:'D7', 7:'D1', 8:'A0', 9:'D2', 10:'A1', 11:'D6', 12:'A2', 13:'D5', 14:'A3',
        15:'D3', 16:'A15B', 17:'D4', 18:'MREQB', 19:'IORQB', 20:'A7P', 21:'RDB', 22:'M1', 23:'WRB', 24:'A8', 25:'A7', 26:'A9',
        27:'A6', 28:'A10', 29:'A5', 30:'A11', 31:'A4', 32:'RFSHB', 33:'BE', 34:'EXROM', 35:'ROSCS', 36:'GND'}
N.pins('J4', dock)
N.note('J4 odd-numbered pins (1-35) read from a 300 dpi crop; 1, 31 and 33 less sure. Pin 20 (A7RB) is the muxed A7 of U11 (A7P).')
# pull-ups on the dock-facing active-low lines the SCLD only receives
N.chip('R75', 'RES'); N.pins('R75', {1: '+5V', 2: 'BE'})
N.note('R75: pull-up on /BE (INFERRED: the schematic has a diode and the P1/J4 BE wires there; reference R75 is in its list of numbers that do not exist).')
