"""Where the parts are on the TS2068 board, from the component layout drawing (Technical Manual
Appendix D, page 353, "TS2068 PC board component layout", 335-850001-03).

The drawing is printed rotated by a quarter turn; the positions here are pixels of the page image
turned upright (rotated 90 degrees clockwise, 1650 x 1275). The ICs, the dock connector J4, the keyboard connector J9, the crystal, the diodes, the
transistors and most of the resistors are placed. J5 and J6 (the joystick connectors) are only
labels on the drawing and are put at the board edge by their diodes; U21 and R75 are not on the drawing
at all and go in a row below the board; the
small positions are good to a few mm, which doesn't matter for the pin-level netlist. ICs are drawn notch up (pin 1 top left). The board
outline is not a rectangle (the lower right corner is cut out for the dock), the generator uses
the bounding box.
"""
ORIGIN = (103, 273)    # top left corner of the board in the upright page image
SCALE = 4.0            # page pixels per mm (from the chips' lengths: 28 to 40 pin DIPs)
SIZE = (1445, 587)     # bounding box of the board, pixels (about 361 x 147 mm)

IC = {
    'U1': (262, 598),
    'U4': (329, 358),
    'U5': (996, 422),
    'U6': (648, 735), 'U7': (722, 735), 'U12': (786, 735), 'U17': (883, 735), 'U13': (975, 735),
    'U18': (1117, 735),
    'U9': (547, 733),
    'U10': (1114, 437), 'U11': (1147, 560),
    'U14': (459, 735),
    'U15': (570, 523),
    'U16': (1053, 573),
    'U19': (685, 563),
    'U20': (1271, 470),
}
# the 68 pin SCLD in its square socket: centre
SQUARE = {'U3': (848, 468)}
# connectors: (x, y, 'h' or 'v' along, rows)
CONN = {
    'J4': (1361, 653, 'h', 2),
    'J9': (521, 355, 'h', 1),
    # the joystick connectors: only their labels are on the drawing (J5 above its diodes D14-D18 at the
    # left edge, J6 beside D19-D23 at the right), so these two places are guesses
    'J5': (125, 470, 'v', 1),
    'J6': (1525, 470, 'v', 1),
}
# axial parts: centre; vertical, or horizontal with a third item
TWO_PAD = {
    # keyboard diodes CR6-CR13 (D6-D13), a row of eight standing up next to L1
    **{'CR%d' % (6 + k): (515 + k * 10.2, 405) for k in range(8)},
    # joystick diodes CR14-CR18 (D14-D18) beside the left connector, lying down
    'CR18': (195, 502, 'h'), 'CR17': (195, 513, 'h'), 'CR16': (195, 523, 'h'), 'CR15': (195, 536, 'h'), 'CR14': (195, 547, 'h'),
    # CR19-CR23 (D19-D23) at the right edge
    'CR22': (1487, 528, 'h'), 'CR21': (1487, 538, 'h'), 'CR20': (1487, 548, 'h'), 'CR19': (1487, 558, 'h'),
    'CR23': (1487, 568, 'h'),
    'R40': (500, 637, 'h'),
    'R42': (748, 592, 'h'), 'R41': (748, 601, 'h'),
    'R43': (383, 658, 'h'), 'R45': (383, 668, 'h'),
    'R44': (1334, 443, 'h'),
}
# transistors
OTHER = {'Y1': (960, 420), 'Q7': (1327, 462), 'Q8': (327, 668)}
