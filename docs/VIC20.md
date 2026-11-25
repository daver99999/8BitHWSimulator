# Commodore VIC-20, ASSY 250403 (cost reduced)

The simulator's 250403 netlist and layout come from jimbro1000's KiCad project of the board (the "Vicky Twenty" replica), not
from Commodore's own drawings. Two errors in it were found only by booting: the 6502's A11 and the block select BLK4 were one
net, and both 6522s were selected by A4.

## What was checked

Against David Green's 250403 (board unpowered, a meter on continuity), every reading as the netlist has it:

- The two fixes: the 6502 A11 (UE10 pin 20) is not joined to BLK4 (UC5 pin 11); the two 6522s' pin 24 lines are not joined, UAB3's
  is on A4 (UE10 pin 13) and UAB1's on A5 (UE10 pin 14); UAB1's pin 23 is on UC6 pin 11.
- The keyboard connector (P10 in the netlist): pin 1 is ground, pin 20 is PA0 (UAB1 pin 2), pin 12 is PB0 (UAB1 pin 10), pin 5 is
  PB7 (UAB1 pin 17), and pin 3 is the RESTORE line on UAB3's CA1 (pin 40).
- The video bus to the colour RAM: the 6560's VD11 (UB7 pin 5) is on UE1 pin 14 and VD8 (UB7 pin 8) on UE1 pin 11.

## Not checked

The 6560 is a new model in the simulator; its clock, sound and analog video are not simulated. The cassette, user port, serial
port, controller port and expansion port, the 7406 and the 74LS245 bus switches, the ROM and RAM wiring (a boot proves most of it),
the power section and the resistor and capacitor values were not measured.
