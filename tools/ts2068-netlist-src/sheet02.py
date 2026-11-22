"""Address buffer U5, DRAM address multiplexers U10/U11, glue U21, A' bus.

READ FROM THE SCHEMATIC (and Technical Manual 2.1.8, Table 2.1.8-1):
 - U5 (74LS244) takes the muxed CPU address A7'..A0' (its input pins 2,8,4,13,17,15,6,11) and drives the display RAM's address bus MA7..MA0
   (its output pins 18,12,16,7,3,5,14,9).  It is on while the CPU addresses the display RAM; the SCLD lets go of MA then (TS).
 - U10 outputs YA,YB,YC,YD = A3',A4',A5',A6'; U11 outputs YB,YD = A1',A2'.  The 74LS157 select is the SCLD's MUX; /G (pin 15) is GND.
   The manual says the 4416 has 8 row and 6 column address bits: A0'-A5' are row A0-A5 then column A8-A13, A6' and A7' only matter as row bits.
 - U11 IIA/IOA (pins 3, 2) ... see below; the 8th row bit A7' comes from the SCLD's ATR (pin 38) through U11.
 MEASURED on a 03D board: DRAM A0 (U12 pin 14) is not connected to U11 pin 12.  So A0' is U11 YA (pin 4) and A7' is YC (pin 12) here.
 INFERRED: which CPU address bit goes to which mux input.
 U5's /OE is the wired-OR of CR27 (TS) and CR28 (U21 pin 11) with R74 pulling it low, traced from the schematic (see the end of this sheet);
 the links H and G are taken as fitted.  On the 03D U5 and U21 are on the daughterboard."""
from netlib import N
N.sheet_is('02')
N.chip('U5', 'LS244'); N.chip('U10', 'LS157'); N.chip('U11', 'LS157'); N.chip('U21', 'LS00')
N.pins('U5', {1:'OEB', 19:'OEB',
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

# U5's /OE (pins 1 and 19) is one node, OEB, through the H link: R74 (390 ohm) pulls it low, CR27 raises it from TS through the G link, and
# CR28 raises it from U21 pin 11.  U21 (the 74LS00 on the daughterboard): pin 3 = NAND(+5V, /WR), pin 8 = NAND(/ROSCS, /BE), pin 6 = NAND(3, 8),
# pin 11 = NAND(6, +5V), so pin 11 = WR AND (ROSCS OR BE): U5 is turned off by a write to the dock or the bus enable.  READ FROM THE SCHEMATIC
# (pages 360 and 361); the H and G links are taken as fitted.  The diodes: pin 1 is the anode, pin 2 the cathode.
N.pins('U21', {1:'+5V', 2:'WR', 3:'U21_3', 4:'U21_3', 5:'U21_8', 6:'U21_6', 8:'U21_8', 9:'ROSCS', 10:'BE', 11:'U21_11', 12:'U21_6', 13:'+5V'})
N.chip('CR27', 'DIODE'); N.pins('CR27', {1: 'TS', 2: 'OEB'})
N.chip('CR28', 'DIODE'); N.pins('CR28', {1: 'U21_11', 2: 'OEB'})
N.chip('R74', 'RES'); N.pins('R74', {1: 'OEB', 2: 'GND'})
