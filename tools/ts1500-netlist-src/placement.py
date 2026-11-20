"""Where the parts are drawn on the TS1500 board. From a photo of the board seen from above, turned a quarter turn so that the edge
connector P1 is at the top (as on the TS1000 boards) and the eight 4116 DRAMs U8-U15 run along the bottom. The positions are pixels of
that turned photo (about 8.3 per mm, from the Z80 and the DRAMs) converted to 4 per mm; they are good to a few mm.

Every part is placed by its silkscreen label, read from close-up photos of the board: the ICs and connectors, the resistors R1, R2,
R16-R22, R24, R25, R28, R31, R32, the capacitors C13, C18, C19 (the one marked C10 on the board, the mic 47pF), C19E (the one marked
C19 on the board, by the EAR jack), C23, C32, C33, C34, the diodes D10-D17 (two stacks of four next to J6: D10-D13 and D14-D17,
top to bottom) and D19, and the transistor Q1. Uncertain: Q1 (its body is hidden by the crystal and a wire), R2 (its footprint is the
one beside an empty C5 footprint), C32 (the cap marked C27 on the board is drawn here). The parts of the power supply and the mods added
to this board (the green buck regulator, wires) are not drawn.

Every DIP is drawn with pin 1 at the bottom right (the converter rotates the pads half a turn, see ROT180): the chips lie with the
lettering upright in the photo, which puts pin 1 at the left of the lettering, and the quarter turn leaves it at the bottom right.
The U5 gate array is drawn as a 68 pin square package whose pins are numbered the standard way (pin 1 at the middle of the top side
before the half turn); its notch is not readable in the photo."""
ORIGIN = (0, 0)
SCALE = 4.0
X0, Y0 = 410, 510          # the turned photo's pixel that is the drawing's top left
PXMM = 8.3                 # pixels of the turned photo per mm
SIZE = (880, 600)
ROT180 = True              # the converter turns every part's pads half a turn about its centre (DIP pin 1 at the bottom right)

def P(x, y):
    """pixel of the turned photo -> pixel of the drawing (4 per mm)"""
    return (round((x - X0) / PXMM * SCALE), round((y - Y0) / PXMM * SCALE))

IC = {'U8': P(493, 1422), 'U9': P(647, 1418), 'U10': P(800, 1416), 'U11': P(951, 1412), 'U12': P(1090, 1411), 'U13': P(1240, 1405),
      'U14': P(1390, 1400), 'U15': P(1548, 1396), 'U2': P(1000, 800), 'U3': P(1137, 787), 'U6': P(1395, 1018), 'U7': P(1637, 962)}
SQUARE = {'U5': P(791, 1028)}
CONN = {'P1': P(1578, 595) + ('h', 2), 'J5': P(692, 1247) + ('h', 1), 'J6': P(1855, 1167) + ('h', 1),
        'MOD': P(582, 667) + ('h', 1), 'J1': P(871, 618) + ('v', 1), 'J2': P(992, 615) + ('v', 1), 'J3': P(2025, 580) + ('v', 1)}
TWO_PAD = {}
def put(ref, x, y, horizontal=False):
    """a two lead part at (x, y) of the turned photo, lying along x (horizontal) or along y"""
    px, py = P(x, y)
    TWO_PAD[ref] = (px, py, 'h') if horizontal else (px, py)

for k, ref in enumerate(['D10', 'D11', 'D12', 'D13']): put(ref, 1662, 1265 + 20 * k, True)    # the two stacks of four next to J6
for k, ref in enumerate(['D14', 'D15', 'D16', 'D17']): put(ref, 1806, 1265 + 20 * k, True)
put('D19', 589, 987); put('R1', 562, 992); put('R2', 550, 900, True)
put('R22', 537, 1196, True); put('R21', 537, 1218, True); put('R32', 565, 1237, True)
put('R31', 545, 1132, True); put('R18', 587, 1145, True); put('R20', 840, 873, True)
put('R16', 860, 727); put('C13', 878, 727); put('C19E', 898, 727)
put('R17', 1150, 602, True); put('C18', 1110, 623, True); put('R19', 1108, 642, True); put('C19', 1110, 662, True)
put('R25', 1200, 1050); put('R24', 1466, 1059); put('R28', 1550, 1206, True)
put('C23', 1767, 982); put('C32', 1175, 1272, True); put('C33', 625, 1534, True); put('C34', 672, 1557, True)
OTHER = {'X1': P(578, 1075), 'Q1': P(550, 1165)}
