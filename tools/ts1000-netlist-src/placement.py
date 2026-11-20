"""Where the parts are on the TS1000/ZX81 Issue 3 board, read from a top photo of an Issue 3 board ("Sinclair ZX81 PCB Top", the
UK Issue 3 with the Ferranti 2C210E ULA, two 2114 RAMs and an Astec modulator: the same board as the Timex's, with the USA parts
R31, R32, D9 and C11/C12 left unfitted but silkscreened). It is drawn as the photo shows it: the edge connector at the top right,
the modulator and the three jacks at the left, the keyboard connectors at the bottom right, chips with pin 1 at the top left.
The photo is 5.2 px per mm (from the length of the Z80, 52 mm) and the drawing 4 px per mm. Positions are good to a mm or two.

Read from the silkscreen in the photo: the column of nine 1K resistors (R18-R26), the box of four 470 ohm (R8-R10, R14), the four 470 ohm
below the Z80 (R12, R11, R7, R13), R43-R47 and D1-D8 along the bottom, C8/C9, and where R27/R34, R31/R32, D9, C10/C11/C12
and the jacks are. NOT read (the labels are under the chips, off the photo or too small): which of the four 10K pull-ups under the Z80 is
which (R40, R39, R42, R41 is the order the data lines D1, D0, D2, D3 and a continuity check give), which of the left four is R35-R38,
and the names of the small resistors round the ULA and at the edge connector: those go by the circuit and are a guess.
The 22 uF (C3) and the 100 uF (C5), the crystal and the two transistors are placed from the photo; the 7805 and the 2114 pair
(drawn here as the one 6116 IC4 between them) are not what the netlist has."""
ORIGIN = (0, 0)
SCALE = 4.0
X0, Y0, PXMM = 80.0, 108.0, 5.2
SIZE = (round((905 - X0) / PXMM * SCALE), round((640 - Y0) / PXMM * SCALE))

def P(x, y):
    """pixel of the photo -> pixel of the board drawing"""
    return (round((x - X0) / PXMM * SCALE), round((y - Y0) / PXMM * SCALE))

IC = {'IC1': P(343, 262), 'IC3': P(470, 348), 'IC2': P(608, 448), 'IC4': P(741, 350)}
CONN = {
    'J1': P(718, 125) + ('h', 2),
    'KB2': P(833, 582) + ('h', 1), 'KB1': P(693, 595) + ('h', 1),
    'EARJ': P(115, 388) + ('v', 1), 'MICJ': P(115, 465) + ('v', 1), 'DC': P(115, 545) + ('v', 1),
    'MOD': P(215, 345) + ('h', 1),
}
TWO_PAD = {}
def put(ref, x, y, horizontal=False):
    px, py = P(x, y)
    TWO_PAD[ref] = (px, py, 'h') if horizontal else (px, py)

for k, ref in enumerate(['R22', 'R26', 'R18', 'R19', 'R25', 'R24', 'R23', 'R20', 'R21']):   # the column of nine 1K, as labelled
    put(ref, 588, 198 + 13.2 * k, True)
for k, ref in enumerate(['R40', 'R39', 'R42', 'R41']):                      # the four 10K under the Z80 (order from the data lines)
    put(ref, 462, 488 + 16 * k, True)
for k, ref in enumerate(['R12', 'R11', 'R7', 'R13']):                       # the four 470 under them, as labelled
    put(ref, 462, 553 + 16 * k, True)
for k, ref in enumerate(['R8', 'R10', 'R14', 'R9']):                        # the box of four 470 left of the ROM
    put(ref, 580, 543 + 15 * k, True)
for k, ref in enumerate(['R35', 'R36', 'R37', 'R38']):                      # the four 10K left of the Z80, below R28/R29
    put(ref, 343, 480 + 13.3 * k, True)
for k in range(5):                                                          # R43-R47, under the label R43->47
    put('R%d' % (43 + k), 700 + 13.2 * k, 511)
for k in range(8):                                                          # D1-D8
    put('D%d' % (1 + k), 767 + 14.3 * k, 508)
put('R28', 343, 427, True); put('R29', 347, 449, True)                      # left of the Z80, above the four
put('R27', 190, 448, True); put('R34', 242, 405, True)                      # by the jacks
put('R6', 400, 300); put('R5', 395, 370); put('R33', 413, 377)              # the vertical ones between the ULA and the Z80
put('R15', 532, 483); put('R30', 548, 488)                                  # the vertical pair left of the ROM
put('R1', 455, 123, True); put('R2', 448, 133, True); put('R4', 443, 143, True)   # the cluster at the top (a fourth resistor there is R3, which the netlist does not have)
put('R16', 810, 193, True); put('R17', 793, 172)                            # by TR1 at the edge connector
put('R32', 270, 268); put('R31', 277, 298); put('D9', 272, 225)             # the USA parts, silkscreen only on this board
put('C1', 418, 112, True); put('C6', 445, 158, True); put('C4', 273, 317); put('C7', 410, 335); put('C2', 395, 452)
put('C10', 207, 421, True); put('C11', 217, 433, True); put('C12', 347, 436, True)
put('C8', 690, 482); put('C9', 865, 478)
OTHER = {'X1': P(408, 288), 'TR2': P(452, 175), 'TR1': P(820, 170), 'C5': P(525, 355), 'C3': P(580, 172)}
