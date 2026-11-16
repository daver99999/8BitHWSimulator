"""Where the parts are on the TS2068 board, from the component layout drawing (Technical Manual
Appendix D, page 353).  Not read yet: every table is empty, so tools/ts2068_netlist2c.py puts the parts
in rows below the board.  Fill IC / CONN / TWO_PAD / OTHER in the same format as tools/pet-netlist-src/placement.py."""
ORIGIN = (0, 0)
SCALE = 3.8            # drawing pixels per mm
SIZE = (1300, 560)     # board size in drawing pixels (about 342 x 147 mm)
IC = {}
CONN = {}
TWO_PAD = {}
OTHER = {}
