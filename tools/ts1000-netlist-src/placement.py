"""Where the parts are drawn on the TS1000 board. NOT from the component layout of an Issue 3 board: a rough layout in
the spirit of the ZX81 (keyboard connectors along the top, the ULA at the left, the edge connector at the right) until
a photo of the real board is available. Pixels, 4 per mm; the board is about 160 x 130 mm."""
ORIGIN = (0, 0)
SCALE = 4.0
SIZE = (640, 520)

IC = {'IC1': (150, 270), 'IC3': (470, 270), 'IC2': (310, 150), 'IC4': (310, 390)}
CONN = {
    'J1': (610, 270, 'v', 2),
    'KB1': (190, 40, 'h', 1), 'KB2': (390, 40, 'h', 1),
    'MIC': (560, 440, 'h', 1), 'EAR': (560, 470, 'h', 1), 'MOD': (80, 460, 'h', 1), 'DC': (80, 430, 'h', 1),
}
TWO_PAD = {}
for k in range(8):                       # keyboard diodes D1-D8 under KB2
    TWO_PAD['D%d' % (1 + k)] = (330 + k * 14, 90)
for k in range(8):                       # 470 ohm data bus resistors between the CPU and the ROM/RAM columns
    TWO_PAD['R%d' % (1 + k)] = (360 + k * 11, 280)
for k in range(9):                       # 1K address resistors
    TWO_PAD['R%d' % (51 + k)] = (200 + k * 9, 280)
for k in range(8):                       # R35-R42 data bus pull-ups
    TWO_PAD['R%d' % (35 + k)] = (120 + k * 12, 480)
for k in range(5):                       # R43-R47 keyboard pull-ups
    TWO_PAD['R%d' % (43 + k)] = (230 + k * 12, 480)
TWO_PAD.update({'R17': (520, 200, 'h'), 'R60': (520, 215, 'h'), 'R61': (520, 230, 'h'), 'R18': (230, 200, 'h'),
                'R19': (230, 215, 'h'), 'R20': (560, 170, 'h'), 'R21': (560, 155, 'h'), 'R22': (560, 125, 'h'),
                'R30': (80, 200, 'h')})
OTHER = {'X1': (110, 130), 'TR1': (520, 125), 'TR2': (560, 105)}
