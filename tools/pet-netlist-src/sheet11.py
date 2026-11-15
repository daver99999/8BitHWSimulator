from netlib import N
N.sheet_is('11')
N.note('S11: power sheet (J10/J11 raw power -> bridges CR2/CR4 -> VR1 7905 (-5V), VR2 7812 (+12V), VR3/VR4 LM340-5 (+5V)). Rails are treated as ideal supplies in the simulation: +5V, +12V, -5V, +9V_UNREG, GND. Decoupling caps (C12.. etc.) omitted from the logic netlist.')
N.note('S11: 4116 pins: -5V pin1, +12V pin8, +5V pin9 supplied by these rails.')
# Power input of the simulation (not a part of the real board's logic diagram: the supplies come in on J10/J11 and
# are regulated on the board). The simulator drives these three nets through this pseudo connector.
N.chip('J7','CONN')
N.pins('J7', {1:'+12V', 2:'-5V', 3:'+9V_UNREG'})
N.note('J7 is a pseudo connector added for the simulator: it drives +12V, -5V and +9V_UNREG (the real board makes them from J10/J11 raw power with VR1, VR2, ...).')

# ---- the real power section (sheet 11), as breakable parts: rails still come from J7, but pet.c derives them from these ----
# VR3/VR4 LM340-5 (1 IN, 2 OUT, 3 GND) make +5V from +9V UNREG; VR2 7812 (1 IN, 2 OUT, 3 GND) makes +12V from the CR4 bridge;
# VR1 7905 (1 GND, 2 IN, 3 OUT) makes -5V from the CR2 bridge's negative side.  CR1/CR5/CR6/CR7: reverse protection across each regulator.
for ref, pins in (('VR3', {1:'+9V_UNREG', 2:'+5V', 3:'GND'}), ('VR4', {1:'+9V_UNREG', 2:'+5V', 3:'GND'}),
                  ('VR2', {1:'RAW_P', 2:'+12V', 3:'GND'}), ('VR1', {1:'GND', 2:'RAW_N', 3:'-5V'})):
    N.chip(ref, 'Q3'); N.pins(ref, pins)
for ref, a, b in (('CR1','RAW_N','-5V'), ('CR5','RAW_P','+12V'), ('CR6','+5V','+9V_UNREG'), ('CR7','+5V','+9V_UNREG')):
    N.chip(ref, 'RES'); N.pins(ref, {1:a, 2:b})
N.chip('CR2', 'BRIDGE'); N.pins('CR2', {1:'CR2_DCP', 2:'AC9_B', 3:'AC9_A', 4:'RAW_N'})
N.chip('CR4', 'BRIDGE'); N.pins('CR4', {1:'RAW_P', 2:'AC16_B', 3:'AC16_A', 4:'GND'})
N.note('S11 parts added as breakable parts: the bridges CR2 (J8 pins 1,4,5: only the negative supply of VR1; +9V UNREG comes straight from the raw power connector) and CR4 (J8 pins 8,9: VR2), the regulators VR1-VR4 and the diodes across them. AC inputs (J8 pins 4,5 and 8,9) are not modelled.')
