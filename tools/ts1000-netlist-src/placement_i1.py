"""Where the parts are on the ZX81/TS1000 Issue 1 board: read from "ZX81 Issue 1 Layout" (drawn by Dave 'Steamy' Briggs, Nov 2023,
spectrumforeveryone.com, kept with credit: not for resale), a drawing of the component side with the edge connector at the top.
The positions are pixels of that drawing rendered at 2000 px wide (10.9 px per mm: 46 edge contacts at 2.54 mm are 633 px long; the
board is 1620 x 1052 px there), converted to 4 px per mm like the Issue 3's. It is a drawing, not a scan: good to a mm or two.
Chips are all pin 1 at the top left, as drawn. R3 and RP2 are not fitted and are left out; C10-C12 and R27 are the USA/Timex parts."""
ORIGIN = (0, 0)
SCALE = 4.0
X0, Y0, PXMM = 197.0, 88.0, 10.9
SIZE = (round((1817 - X0) / PXMM * SCALE), round((1140 - Y0) / PXMM * SCALE))

def P(x, y):
    return (round((x - X0) / PXMM * SCALE), round((y - Y0) / PXMM * SCALE))

IC = {'IC1': P(637, 440), 'IC3': P(1151, 605), 'IC2': P(872, 948), 'IC4': P(1665, 567)}
# connectors: centre, orientation, rows. J1 is made in ts1000i1_netlist2c (the A side is the lower row here).
CONN = {
    'J1': P(1500, 130) + ('h', 2),
    'KB1': P(1425, 1100) + ('h', 1), 'KB2': P(1650, 1100) + ('h', 1),
    'DC': P(255, 950) + ('v', 1), 'MICJ': P(255, 795) + ('v', 1), 'EARJ': P(255, 648) + ('v', 1),
    'MOD': P(364, 480) + ('v', 1),
}
SIP = {                       # a resistor pack: centre, orientation of its row, pin 1 end (the dot): 'b' bottom / 'r' right
    'RP1': P(1025, 614) + ('v', 'b'),
    'RP3': P(1430, 1042) + ('h', 'r'),
}
TWO_PAD = {}
def put(ref, x, y, horizontal=False):
    px, py = P(x, y)
    TWO_PAD[ref] = (px, py, 'h') if horizontal else (px, py)

put('R1', 605, 117, True); put('C1', 605, 153, True)
put('R2', 747, 225); put('C2', 853, 140); put('C4', 780, 240); put('R4', 826, 350)
put('C6', 743, 342); put('C7', 897, 412, True); put('R5', 933, 347, True); put('R6', 933, 378, True)
put('R15', 1380, 303); put('R16', 1465, 303); put('R17', 1522, 303)
for k, ref in enumerate(['R18', 'R19', 'R20', 'R21', 'R22']): put(ref, 1305 + 36 * k + 36, 832, True)   # right of the CPU, above the second row
for k, ref in enumerate(['R23', 'R24', 'R25', 'R26']): put(ref, 1350 + 36 * k + 36, 881, True)
for k, ref in enumerate(['R7', 'R8', 'R9', 'R10']): put(ref, 842, 540 + 30 * k, True)     # the box of eight 470R: R7-R10 on its left,
for k, ref in enumerate(['R11', 'R12', 'R13', 'R14']): put(ref, 905, 540 + 30 * k, True)  # R11-R14 on its right
put('R28', 660, 944, True); put('R29', 440, 944, True); put('R33', 525, 783, True); put('R34', 499, 653)
put('R30', 590, 893, True); put('R31', 449, 911, True); put('R32', 440, 879, True); put('D9', 425, 838, True)
put('R27', 248, 887, True); put('C10', 357, 707); put('C11', 373, 806, True); put('C12', 375, 778, True)
put('C8', 1210, 898, True); put('C9', 1690, 758, True)
for k, ref in enumerate(['D1', 'D2', 'D3', 'D4', 'D5', 'D6', 'D8', 'D7']): put(ref, 1553 + 32 * k, 1005)
OTHER = {'X1': P(783, 340), 'TR1': P(720, 112), 'TR2': P(883, 297), 'C3': P(915, 128), 'C5': P(848, 232)}
