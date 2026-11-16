"""Where the parts are drawn on the TS1500 board: A ROUGH LAYOUT, not read from a board (no photo of a TS1500 board yet).
The edge connector is at the bottom, the keyboard connectors at the top, the jacks and the modulator at the right, the 8
DRAMs in a row above the edge connector. The U5 gate array is drawn as a 68 pin square package whose pins are numbered
the standard way (pin 1 at the middle of the top side); the pin numbers of this netlist are INFERRED from the schematic's
symbol and are not the real ones. Parts not placed go in a row below the board."""
ORIGIN = (0, 0)
SCALE = 4.0
SIZE = (640, 480)
IC = {'U1': (520, 170), 'U2': (120, 120), 'U3': (120, 280), 'U6': (210, 280),
      'U10': (40, 370), 'U9': (100, 370), 'U8': (160, 370), 'U11': (220, 370),
      'U12': (280, 370), 'U13': (340, 370), 'U14': (400, 370), 'U15': (460, 370)}
SQUARE = {'U5': (330, 170)}
CONN = {
    'P1': (300, 450, 'h', 2),
    'KB2': (120, 20, 'h', 1), 'KB1': (260, 20, 'h', 1),
    'DC': (610, 60, 'v', 1), 'MICJ': (610, 140, 'v', 1), 'EARJ': (610, 220, 'v', 1),
    'MOD': (590, 320, 'h', 1),
}
TWO_PAD = {}
OTHER = {'X1': (430, 60)}
