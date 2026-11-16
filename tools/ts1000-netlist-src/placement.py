"""Where the parts are drawn on the TS1000 board. From a photo of an Issue 3 board ("TIMEX SINCLAIR TS 1000 ISS 3"),
taken with the edge connector on the right; here it is turned a quarter turn clockwise (edge connector at the bottom,
jacks and modulator at the right, keyboard connectors at the top).

The positions are photo pixels (about 10 per mm) turned and scaled to 4 pixels per mm. They are good to a few mm.
That board has three mods that are NOT drawn: a switching regulator (top left), a 'ZX81 FORTH' ROM adapter over IC2 and a
32K RAM (Sony CXK58256P) wired in next to the empty stock RAM position IC4. The stock IC4 position is placed where
its bare pads are. Within a group of resistors the order is NOT read from the
silkscreen (the references come from the ZX81 schematic: R7-R14 470 ohm, R18-R26 1K);
R17, R27, R31, R32, R34, R43-R47 and D9 were seen on the board. The capacitors, R4, R29, R30, R33, C-values and the
small parts not placed here go in a row below the board. X1 (a 6.5 MHz ceramic filter) is probably the flat brown part beside TR2."""
ORIGIN = (0, 0)
SCALE = 4.0
SIZE = (630, 412)

def P(px, py):
    """photo pixel -> board pixel (turned 90 degrees clockwise, 0.4 board pixels per photo pixel)"""
    return (round((1850 - py) * 0.4), round((px - 185) * 0.4))

IC = {'IC1': P(920, 755), 'IC3': P(745, 1005), 'IC2': P(560, 1285), 'IC4': P(730, 1432)}
CONN = {
    'J1': P(1170, 1450) + ('h', 2),
    'KB1': P(225, 1445) + ('h', 1), 'KB2': P(290, 1720) + ('h', 1),
    'MICJ': P(320, 350) + ('v', 1), 'EARJ': P(500, 350) + ('v', 1), 'DC': P(660, 350) + ('v', 1),
    'MOD': P(1030, 400) + ('v', 1),
}
TWO_PAD = {}
def put(ref, px, py, horizontal_in_sim=False):
    x, y = P(px, py)
    TWO_PAD[ref] = (x, y, 'h') if horizontal_in_sim else (x, y)
for k, px in enumerate([370, 405, 432, 458, 490, 518, 545, 572]):      # the row at the top left: R35-R42
    put('R%d' % (35 + k), px, 770, True)
for k, px in enumerate([237, 267, 293, 327, 355, 380, 410, 437]):      # 8 resistors, left: the 470 ohm data resistors R7-R14
    put('R%d' % (7 + k), px, 1010, True)
for k, px in enumerate([237, 267, 295, 327]):                          # 4 resistors, left, below: the address resistors R23-R26
    put('R%d' % (23 + k), px, 1210, True)
for k, px in enumerate([885, 910, 935, 962, 985]):                     # a row right of the ROM: the address resistors R18-R22
    put('R%d' % (18 + k), px, 1215, True)
for k in range(5):                                                     # R43-R47, beside the keyboard connector
    put('R%d' % (43 + k), 440, 1455 + 23 * k)
for k in range(8):                                                     # D1-D8, below them
    put('D%d' % (1 + k), 425, 1590 + 21 * k)
put('R2', 665, 873); put('R28', 665, 897)                              # the pairs between the ULA and the Z80 (680 ohm)
put('R16', 850, 872); put('R15', 860, 896)
put('R31', 790, 640); put('R32', 990, 622); put('D9', 990, 650)        # the USA strap parts near the ULA, seen on the board
put('R27', 493, 575, True); put('R34', 605, 575, True)                 # near the jacks, seen on the board
put('R17', 1025, 1630, True)                                           # the /WAIT pull-up, next to its transistor TR1
put('R1', 1112, 1650, True)
put('R5', 1160, 985, True); put('R6', 1110, 1010, True)                # near TR2 (positions of R5/R6 are a guess)
OTHER = {'C5': P(1075, 1220), 'X1': P(1125, 972), 'TR1': P(1075, 1665), 'TR2': P(1068, 962)}
