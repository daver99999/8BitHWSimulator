"""Where the parts are drawn on the ZX80 board. From a photo of an Issue 2 board seen from above, with the edge connector at the top,
the modulator and the three jacks at the top right and the keyboard membrane below: that is how it is drawn here. The positions are
pixels of that photo (about 8.8 per mm, from the Z80's length), converted to 4 per mm; they are good to a few mm.

Every IC is drawn with pin 1 at the top left (the notches are at the top on the photo). Parts are placed by the silkscreen labels read
from close-up photos: R1-R3, R9-R12 (the horizontal ones left of the Z80), R4-R8 (the five tilted ones), R13-R15, R16-R17, R18-R19,
R20, R21-R23, R24, R25, R27, R28, R29-R33, R34-R35, the diodes D3-D10 (the row above the keyboard, left to right) and D11, the
capacitors C9, C10 (the electrolytic), C11, C15 and the crystal X1. Not placed: the keyboard (it is a membrane ribbon, there are no
connectors on the board), the power supply, the modulator, the jacks and the edge connector (not simulated); R36, R37, R26 and
D1, D2 are empty footprints on this board."""
ORIGIN = (0, 0)
SCALE = 4.0
X0, Y0 = 270, 140          # the photo's pixel that is the drawing's top left
PXMM = 8.8                 # pixels of the photo per mm
SIZE = (640, 540)

def P(x, y):
    """pixel of the photo -> pixel of the drawing (4 per mm)"""
    return (round((x - X0) / PXMM * SCALE), round((y - Y0) / PXMM * SCALE))

IC = {'IC1': P(801, 652), 'IC2': P(1430, 578), 'IC3': P(360, 497), 'IC4': P(492, 497), 'IC5': P(1042, 775),
      'IC6': P(1193, 540), 'IC7': P(1205, 782), 'IC8': P(1038, 540), 'IC9': P(482, 743), 'IC10': P(348, 745),
      'IC11': P(1028, 1057), 'IC12': P(1176, 1057), 'IC13': P(750, 1065), 'IC14': P(348, 1065), 'IC15': P(478, 1065),
      'IC16': P(628, 1065), 'IC17': P(877, 1065), 'IC18': P(1298, 1057), 'IC19': P(1494, 807), 'IC20': P(1408, 1057),
      'IC21': P(1372, 807)}
CONN = {}
TWO_PAD = {}
def put(ref, x, y, horizontal=False):
    """a two lead part at (x, y) of the photo, lying along x (horizontal) or along y"""
    px, py = P(x, y)
    TWO_PAD[ref] = (px, py, 'h') if horizontal else (px, py)

for k, (x, y) in enumerate([(625, 525), (633, 555), (643, 582), (650, 612), (657, 645)]):    # R4-R8: the five tilted ones
    put('R%d' % (4 + k), x, y, True)
put('R9', 612, 713, True); put('R10', 612, 745, True); put('R11', 612, 780, True); put('R12', 612, 828, True)
put('R1', 372, 635, True); put('R2', 370, 878, True); put('R3', 368, 908, True)
put('R13', 348, 1193, True); put('R14', 348, 1225, True); put('R15', 348, 1256, True)
put('R16', 505, 1200, True); put('R17', 505, 1227, True); put('R18', 670, 1192, True); put('R19', 660, 1231, True)
put('R20', 550, 1033); put('R24', 950, 1138); put('R25', 1105, 1105)
put('R21', 915, 828); put('R22', 945, 825); put('R23', 972, 845)
put('R27', 1510, 1030); put('R28', 1507, 645)
for k, y in enumerate([285, 310, 333]): put('R%d' % (29 + k), 1300, y, True)               # R29-R31, R32 and R33 below
put('R32', 1292, 362, True); put('R33', 1292, 385, True)
put('R34', 1425, 302, True); put('R35', 1425, 327, True)
for ref, x in zip(['D3', 'D4', 'D5', 'D6', 'D7', 'D8', 'D9', 'D10'], [715, 825, 955, 1050, 1170, 1275, 1390, 1485]):
    put(ref, x, 1252, True)                                                                  # the row above the keyboard
put('D11', 418, 715)
put('C10', 940, 665)                                                                         # the electrolytic, standing
OTHER = {'X1': P(1520, 1120), 'C9': P(488, 958), 'C11': P(1122, 1000), 'C15': P(1518, 1178)}
