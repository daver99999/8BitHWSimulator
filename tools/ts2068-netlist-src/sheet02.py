"""Address buffer U5, DRAM address multiplexers U10/U11, glue U21, A' bus.

READ FROM THE SCHEMATIC (and Technical Manual 2.1.8, Table 2.1.8-1):
 - U5 (74LS244) takes the muxed CPU address A7'..A0' (its input pins 2,8,4,13,17,15,6,11) and drives the display RAM's address bus MA7..MA0
   (its output pins 18,12,16,7,3,5,14,9).  It is on while the CPU addresses the display RAM; the SCLD lets go of MA then (TS).
 - U10 outputs YA,YB,YC,YD = A3',A4',A5',A6'; U11 outputs YB,YD = A1',A2'.  The 74LS157 select is the SCLD's MUX; /G (pin 15) is GND.
   The manual says the 4416 has 8 row and 6 column address bits: A0'-A5' are row A0-A5 then column A8-A13, A6' and A7' only matter as row bits.
 - U11 IIA/IOA (pins 3, 2) ... see below; the 8th row bit A7' comes from the SCLD's ATR (pin 38) through U11.
 MEASURED on a 03D board: DRAM A0 (U12 pin 14) is not connected to U11 pin 12.  So A0' is U11 YA (pin 4) and A7' is YC (pin 12) here.
 INFERRED: which CPU address bit goes to which mux input; the 74LS00 U21 and the wired-OR on U5's /OE (CR27, CR28, R74, jumpers H/G) were not
 followed: U5's /OE pins are tied to TS here (same as U9's /E), U21 is left unconnected.  The 03D has an extra patch board here (see the README)."""
from netlib import N
N.sheet_is('02')
N.chip('U5', 'LS244'); N.chip('U10', 'LS157'); N.chip('U11', 'LS157'); N.chip('U21', 'LS00')
N.pins('U5', {1:'TS', 19:'TS',
              18:'MA7', 12:'MA6', 16:'MA5', 7:'MA4', 3:'MA3', 5:'MA2', 14:'MA1', 9:'MA0',
              2:'A7P', 8:'A6P', 4:'A5P', 13:'A4P', 17:'A3P', 15:'A2P', 6:'A1P', 11:'A0P'})
# 74LS157 pins: 1A=2 1B=3 1Y=4 / 2A=5 2B=6 2Y=7 / 3A=11 3B=10 3Y=9 / 4A=14 4B=13 4Y=12 ; A = selected while MUX is low (row), B while high (column)
N.pins('U10', {1:'MUX', 15:'GND',
               4:'A3P',  2:'A3',  3:'A11',     # YA
               7:'A4P',  5:'A4',  6:'A12',     # YB
               12:'A5P', 14:'A5', 13:'A13',    # YC
               9:'A6P',  11:'A6', 10:'GND'})   # YD
N.pins('U11', {1:'MUX', 15:'GND',
               4:'A0P',  2:'A0',  3:'A8',      # YA
               7:'A1P',  5:'A1',  6:'A9',      # YB
               9:'A2P',  11:'A2', 10:'A10',    # YD
               12:'A7P', 14:'A7R', 13:'A7R'})  # YC: both inputs on ATR
N.note('A7 row bit: U11 YC = ATR (SCLD pin 38) in both phases (INFERRED from the joined inputs 13/14 on the drawing).')
