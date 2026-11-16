"""Where the parts are drawn on the TS1000 board. From a scan of an Issue 3 ZX81 board (UK, 2114 RAMs, but the same board as
the Timex's: the layout, the reference numbers and the silkscreen are the same) with the edge connector at the bottom, the
jacks and the modulator at the right and the keyboard connectors at the top: that is how it is drawn here. The positions are
pixels of that scan (about 5.4 per mm, from the length of the edge connector), converted to 4 per mm. They are good to a few mm.

Which resistor of a group is where is NOT read from the silkscreen (the pictures are not sharp enough): inside a group the
references go in order. The groups are: R7-R14 (470 ohm) the two columns of 4 next to the ROM and above the Z80, R18-R26 (1K)
the column of 9 below the ROM, R35-R42 (10K) the 8 right of the Z80, R43-R47 below KB1, D1-D8 below KB2. R16, R17, R1 and TR1
are the group next to the edge connector, R2/R28 the pair of vertical resistors between the Z80 and the ULA, X1 and TR2 are
the orange and the flat black part below the ULA's left side. The capacitors and a few resistors (C1-C4, C6-C12, R29, R30, R33)
are not placed from the scan and go in a row below the board. The Timex board in the photos has a Forth ROM adapter over IC2,
a switching regulator and a 32K RAM added: none is drawn."""
ORIGIN = (0, 0)
SCALE = 4.0
SIZE = (603, 441)
S = 4.0 / 5.4

def P(x, y):
    """pixel of the scan -> pixel of the board drawing (the board's top left corner is at 30, 262 in the scan)"""
    return (round((x - 30) * S), round((y - 262) * S))

IC = {'IC1': P(603, 670), 'IC3': P(472, 567), 'IC2': P(331, 454), 'IC4': P(190, 563)}
CONN = {
    'J1': P(185, 826) + ('h', 2),
    'KB2': P(111, 303) + ('h', 1), 'KB1': P(254, 295) + ('h', 1),
    'DC': P(832, 348) + ('v', 1), 'MICJ': P(832, 438) + ('v', 1), 'EARJ': P(832, 527) + ('v', 1),
    'MOD': P(775, 610) + ('h', 1),
}
TWO_PAD = {}
def put(ref, x, y, horizontal=False):
    px, py = P(x, y)
    TWO_PAD[ref] = (px, py, 'h') if horizontal else (px, py)

for k, ref in enumerate(['R7', 'R8', 'R9', 'R10']):                        # the box of 4 next to the ROM
    put(ref, 370, 300 + 15 * k, True)
for k, ref in enumerate(['R11', 'R12', 'R13', 'R14']):                     # the top four of the column above the Z80
    put(ref, 479, 298 + 15 * k, True)
for k in range(9):                                                         # R18-R26: the column of 9 below the ROM
    put('R%d' % (18 + k), 345, 617 + 15 * k, True)
for k in range(8):                                                         # R35-R42: the 8 right of the Z80
    put('R%d' % (35 + k), 595, 364 + 15 * k, True)
for k, x in enumerate([174, 193, 214, 229, 243]):                          # R43-R47, below KB1
    put('R%d' % (43 + k), x, 387)
for k, x in enumerate([50, 65, 80, 95, 110, 125, 140, 153]):               # D1-D8, below KB2
    put('D%d' % (1 + k), x, 391)
put('R2', 535, 528); put('R28', 548, 523)                                  # the 680 ohm pair, vertical
put('R4', 533, 635); put('R15', 551, 631)                                  # the second vertical pair
put('R6', 470, 790, True); put('R5', 474, 806, True)                       # next to X1 and TR2
put('R1', 95, 724, True); put('R16', 95, 763, True); put('R17', 88, 780, True)   # next to TR1 at the edge connector
put('D9', 676, 615); put('R32', 683, 670); put('R31', 672, 720)            # the USA parts, right of the ULA
put('R34', 749, 445, True); put('R27', 697, 491, True)                     # near the jacks
OTHER = {'X1': P(505, 770), 'TR2': P(470, 752), 'TR1': P(93, 748), 'C5': P(329, 769), 'C3': P(725, 362)}
