"""Address buffer U5, DRAM address multiplexers U10/U11, glue U21, A' bus.
READ FROM THE SCHEMATIC: U5 inputs MA7..MA0 (pins 18,12,16,7,3,5,14,9), outputs A7'..A1',A0' (2,8,4,13,17,15,6,11);
U5 /OE pins 1+19 tied together to a node pulled to GND by R74 390R and driven through diode CR28 by U21 pin 11;
U10 outputs YD,YC,YB,YA = A6',A5',A4',A3'; U11 outputs YD,YB = A2',A1'; U11 IIA = A7R (SCLD pin 38); /G pins to GND.
INFERRED (the inputs are unlabelled bus lines): which CPU address bit goes to which mux input, and U11 YA/YC = A0'/A7'.
The schematic has U5 and the 157s both driving the A' bus; the 157 is not tri-state, so this needs checking against the real board."""
from netlib import N
N.sheet_is('02')
N.chip('U5', 'LS244'); N.chip('U10', 'LS157'); N.chip('U11', 'LS157'); N.chip('U21', 'LS00')
# U5: inputs (1A1..1A4, 2A1..2A4) = MA7..MA0, outputs A7'..A0'
N.pins('U5', {1:'U5OE', 19:'U5OE',
              18:'MA7', 12:'MA6', 16:'MA5', 7:'MA4', 3:'MA3', 5:'MA2', 14:'MA1', 9:'MA0',
              2:'A7P', 8:'A6P', 4:'A5P', 13:'A4P', 17:'A3P', 15:'A2P', 6:'A1P', 11:'A0P'})
N.chip('R74', 'RES'); N.pins('R74', {1: 'U5OE', 2: 'GND'})
N.chip('CR28', 'DIODE'); N.pins('CR28', {'1': 'U21_11', '2': 'U5OE'})
# U10 / U11: select = MUX (SCLD pin 21) INFERRED, /G low
N.pins('U10', {1:'MUX', 15:'GND', 9:'A6P', 12:'A5P', 7:'A4P', 4:'A3P'})
N.pins('U11', {1:'MUX', 15:'GND', 9:'A2P', 7:'A1P'})
# INFERRED mux inputs: row (select low) = CPU A(n), column (select high) = CPU A(n+7)
rowcol = {('U10',2,3,'A3P'):None}
mux = {  # (A-in pin, B-in pin, CPU row bit, CPU column bit) by output pin; 74LS157: 1A=2 1B=3 1Y=4 / 2A=5 2B=6 2Y=7 / 3A=11 3B=10 3Y=9 / 4A=14 4B=13 4Y=12
 'U10': {4:(2,3,3,10), 7:(5,6,4,11), 9:(11,10,5,12), 12:(14,13,6,13)},
 'U11': {7:(5,6,1,8), 9:(11,10,2,9)},
}
for ref, outs in mux.items():
    for ypin, (pa, pb, row, col) in outs.items():
        N.pins(ref, {pa: 'A%d' % row, pb: 'A%d' % col})
N.note('U10/U11 data inputs: row = A(n), column = A(n+7), INFERRED. U11 YA (pin 4) = A7P (dock A7RB): input 1B (pin 3) = A7R from the SCLD, 1A (pin 2) = A0 INFERRED; YC (pin 12) inputs 13 and 14 joined, INFERRED = A0P source.')
N.pins('U11', {2: 'A0', 3: 'A7R', 4: 'A7P', 12: 'U11_YC', 13: 'A7', 14: 'A7'})
N.note('MEASURED on a 03D board: DRAM A0 (U12 pin 14) is NOT connected to U11 pin 12, so U11 YC is not A0P; A0P is driven by U5 pin 11 (and the 03D daughterboard).')
# U21 74LS00: only part of the sheet read (gates feeding CR28 / wired-OR with CR27); inputs INFERRED
N.pins('U21', {1:'+5V', 2:'MREQ', 3:'U21_3', 4:'U21_3', 5:'U21_3', 6:'U21_6', 9:'RFSH', 10:'MREQ', 8:'U21_8',
               12:'U21_6', 13:'+5V', 11:'U21_11'})
N.note('U21 connections beyond pins 11 (out), 6, 3, 8 are INFERRED from the drawing (74LS00, 4 gates: 1-2/3, 4-5/6, 9-10/8, 12-13/11).')
