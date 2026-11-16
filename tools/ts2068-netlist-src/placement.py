"""Where the parts are on the TS2068 board, from the component layout drawing (Technical Manual
Appendix D, page 353, "TS2068 PC board component layout", 335-850001-03).

The drawing is printed rotated by a quarter turn; the positions here are pixels of the page image
turned upright (rotated 90 degrees clockwise, 1650 x 1275). Only the ICs, the dock connector J4, the
keyboard connector J9 and the crystal are placed; the resistors, capacitors, diodes and the other
connectors go in a row below the board (the drawing's small parts are too crowded to read reliably),
which doesn't matter for the pin-level netlist. ICs are drawn notch up (pin 1 top left). The board
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
}
TWO_PAD = {}
OTHER = {'Y1': (960, 420)}
