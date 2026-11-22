"""Where the parts are drawn on the TS2068 board. From a photo of the user's 03D board (335-850001-03D), turned a half turn so
that it matches the drawing: the memories along the bottom, the dock J4 at the right, the keyboard connector at the top. The
positions are pixels of that photo (about 5.6 per mm, from the Z80's length and the board's width), converted to 4 per mm;
they are good to a few mm, which doesn't matter for the pin-level netlist.

Placed from the photo: every IC, the SCLD U3, the dock J4, the connectors J5, J6 and J9, the crystal Y1, the diodes CR6-CR23 and
the resistors R41, R42, R44 and the transistor Q7 (read from the silkscreen). R40, R43, R45, Q8 are not found on the photos:
they keep the places they had from the Technical Manual's component layout drawing (Appendix D, page 353), moved to this
frame. U5 and U21 are on the 03D's patch daughterboard (335-80006), R75 is not on the drawing: they go in a row below the board.
ICs are drawn notch up (pin 1 top left). The board outline is not a rectangle (the lower right corner is cut out for the dock);
the generator uses the bounding box.
"""
ORIGIN = (0, 0)
SCALE = 4.0
X0, Y0 = 135, 420          # the photo's pixel that is the drawing's top left
PXMM = 5.6                 # pixels of the photo per mm
SIZE = (1429, 604)         # bounding box of the board, drawing pixels (about 357 x 151 mm)

def P(x, y):
    """pixel of the photo -> pixel of the drawing (4 per mm)"""
    return (round((x - X0) / PXMM * SCALE), round((y - Y0) / PXMM * SCALE))

IC = {
    'U6': P(907, 1089), 'U7': P(1009, 1089), 'U12': P(1095, 1089), 'U17': P(1237, 1095), 'U13': P(1363, 1095),
    'U18': P(1567, 1095),
    'U9': P(763, 1051), 'U14': P(642, 1078), 'U15': P(799, 780),
    'U10': P(1560, 691), 'U11': P(1598, 857),
    'U16': P(1477, 868), 'U19': P(963, 830), 'U20': P(1778, 727),
}
# the 68 pin SCLD in its square socket: centre
SQUARE = {'U3': P(1197, 747)}
# connectors: (x, y, 'h' or 'v' along, rows)
CONN = {
    'J4': (*P(1905, 922), 'h', 2),
    'J9': (*P(743, 567), 'h', 1),
    'J5': (*P(257, 710), 'v', 1),
    'J6': (*P(2050, 712), 'v', 1),
}
TWO_PAD = {}
def put(ref, x, y, horizontal=False):
    """a two lead part at (x, y) of the photo, lying along x (horizontal) or along y"""
    px, py = P(x, y)
    TWO_PAD[ref] = (px, py, 'h') if horizontal else (px, py)

for k in range(8):                                   # CR6-CR13: the row of eight standing up next to L1
    put('CR%d' % (6 + k), 750 + 12.7 * k, 628)
for ref, y in zip(['CR18', 'CR17', 'CR16', 'CR15', 'CR14'], [763, 780, 794, 811, 824]):   # beside J5, lying down
    put(ref, 290, y, True)
for ref, y in zip(['CR22', 'CR21', 'CR20', 'CR19', 'CR23'], [810, 823, 837, 850, 865]):   # beside J6
    put(ref, 2062, y, True)
put('R41', 1045, 917, True); put('R42', 1045, 927, True)
put('R44', 1830, 690, True)
# not found on the photos: the places from the manual's drawing
put('R40', 698, 947, True); put('R43', 533, 976, True); put('R45', 533, 990, True)
OTHER = {'Y1': P(1362, 640), 'Q7': P(1858, 712), 'Q8': P(455, 990)}
