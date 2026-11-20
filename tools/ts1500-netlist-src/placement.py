"""Where the parts are drawn on the TS1500 board. From the photo of the board seen from above (the edge connector P1 at the right,
the eight 4116 DRAMs in a column at the left, the modulator at the top right), turned a quarter turn so that the edge connector is
at the bottom as on the other boards. The positions are pixels of that photo (about 5.3 per mm, from the Z80 and the DRAMs)
converted to 4 per mm; they are good to a few mm. Every DIP is drawn with pin 1 at the top left; the DRAMs, the Z80 and the other
chips lie with the lettering upright in the photo, which puts pin 1 at the left of the lettering: that is where the quarter turn
leaves it. The U5 gate array is drawn as a 68 pin square package whose pins are numbered the standard way (pin 1 at the middle of
the top side); its notch is not readable in the photo.

Which of a group is which is NOT read from the silkscreen (too small): the diodes D10-D17 are the two stacks of four next to J6
(D10-D13 and D14-D17, in order), and the small parts around the modulator and the jacks (R16-R22, R31, R32, C13, C18, C19, C19E, Q1)
are put where the photo has a part of that kind, not by their labels. The green buck regulator, the wires and the other parts added
to this board are modifications, not drawn."""
ORIGIN = (0, 0)
SCALE = 4.0
SIZE = (830, 535)
K = 4.0 / 5.3
ROT180 = True      # the drawing is turned half a turn from the photo's quarter turn: edge connector at the top, as on the TS1000 boards

def P(px, py):
    """pixel of the photo (as reduced in tools notes: board interior 95..790 across, 40..1125 down) -> pixel of the drawing"""
    x, y = round((1125 - py) * K), round((px - 95) * K)
    return (SIZE[0] - x, SIZE[1] - y) if ROT180 else (x, y)

IC = {'U8': P(232, 95), 'U9': P(232, 185), 'U10': P(232, 280), 'U11': P(232, 375), 'U12': P(232, 465), 'U13': P(232, 560),
      'U14': P(232, 650), 'U15': P(232, 745), 'U2': P(610, 405), 'U3': P(615, 490), 'U6': P(490, 650), 'U7': P(505, 800)}
SQUARE = {'U5': P(465, 265)}
CONN = {'P1': P(735, 785) + ('h', 2), 'J5': P(330, 210) + ('h', 1), 'J6': P(383, 930) + ('h', 1),
        'MOD': P(680, 150) + ('h', 1), 'J2': P(725, 330) + ('h', 1), 'J1': P(725, 400) + ('h', 1), 'J3': P(735, 1035) + ('h', 1)}
TWO_PAD = {}
def put(ref, px, py, photo_horizontal=False):
    x, y = P(px, py)
    TWO_PAD[ref] = (x, y) if photo_horizontal else (x, y, 'h')     # a quarter turn: horizontal in the photo is vertical here

for k, ref in enumerate(['D10', 'D11', 'D12', 'D13']): put(ref, 285 + 13 * k, 810)      # the two stacks of four next to J6
for k, ref in enumerate(['D14', 'D15', 'D16', 'D17']): put(ref, 285 + 13 * k, 900)
put('D19', 495, 150, True); put('R1', 495, 130, True); put('R2', 545, 165)
put('R22', 352, 118); put('R32', 338, 130); put('R21', 350, 158); put('R31', 395, 125)
put('R18', 405, 200, True); put('R20', 565, 298)
put('R16', 640, 322, True); put('R19', 650, 340, True); put('R17', 690, 330, True)
put('R25', 715, 470); put('R28', 730, 500); put('R24', 655, 745)
OTHER = {'X1': P(435, 140), 'Q1': P(376, 162), 'C23': P(492, 878), 'C32': P(318, 520), 'C33': P(320, 715), 'C34': P(460, 730),
         'C13': P(655, 355), 'C18': P(670, 320), 'C19': P(648, 375), 'C19E': P(600, 345)}
