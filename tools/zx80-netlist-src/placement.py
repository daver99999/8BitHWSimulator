"""Where the parts are drawn on the ZX80 board. From a top-down scan of an Issue 3 ZX80 board (886 x 1203 pixels, about 6.0 per
mm, from the length of the Z80 (52 mm)) with the edge connector at the top, the modulator and the jacks at the top right and the
keyboard below: that is how it is drawn here. The positions are pixels of that scan, converted to 4 per mm; they are good to a
few mm. Every IC is drawn with pin 1 at the top left (the notch is not readable on the scan).

Not read from the silkscreen (too small): the order inside the groups. R4-R8 are the five diagonal resistors left of the Z80 and
R9-R11 the three horizontal ones below them (R12 is the fourth of that column); D3-D10 are the row of 8 diodes above the keyboard;
R13-R15 and R16-R18 are the groups at the lower left, and R19, R20, R24, R25 are placed near the chips they serve (their silkscreen
is not legible). The two keyboard connectors (KB1, KB2) are under the keyboard membrane, drawn below the board. The power supply,
modulator, jacks and the edge connector are not placed (they are not simulated)."""
ORIGIN = (0, 0)
SCALE = 4.0
SIZE = (580, 560)
S = 4.0 / 6.0

def P(x, y):
    """pixel of the scan -> pixel of the board drawing (the board's top left corner is at 10, 10 in the scan)"""
    return (round((x - 10) * S), round((y - 10) * S))

IC = {'IC1': P(362, 412), 'IC2': P(767, 358), 'IC3': P(82, 310), 'IC4': P(162, 310), 'IC5': P(515, 496),
      'IC6': P(622, 330), 'IC7': P(622, 488), 'IC8': P(510, 332), 'IC9': P(162, 478), 'IC10': P(82, 480),
      'IC11': P(507, 680), 'IC12': P(597, 680), 'IC13': P(332, 680), 'IC14': P(97, 685), 'IC15': P(165, 685),
      'IC16': P(262, 685), 'IC17': P(412, 680), 'IC18': P(675, 680), 'IC19': P(810, 522), 'IC20': P(753, 682),
      'IC21': P(733, 522)}
CONN = {'KB2': P(330, 850) + ('h', 1), 'KB1': P(620, 850) + ('h', 1)}
TWO_PAD = {}
def put(ref, x, y, horizontal=False):
    px, py = P(x, y)
    TWO_PAD[ref] = (px, py, 'h') if horizontal else (px, py)

for k, y in enumerate([322, 340, 358, 376, 394]):                           # R4-R8: the five tilted ones left of the Z80
    put('R%d' % (4 + k), 252, y, True)
for k, y in enumerate([455, 478, 505]):                                     # R9-R11, then R12 (1K pull-up of /NMI)
    put('R%d' % (9 + k), 250, y, True)
put('R12', 250, 535, True)
put('R1', 80, 400, True)
put('R2', 95, 563, True); put('R3', 95, 596, True)
put('R13', 70, 762, True); put('R14', 70, 779, True); put('R15', 70, 797, True)   # the three at the lower left
put('R16', 185, 763, True); put('R17', 130, 780, True); put('R18', 270, 765, True)
put('R19', 440, 745, True); put('R20', 212, 660); put('R24', 458, 710); put('R25', 550, 655)
put('R21', 430, 520); put('R22', 445, 530); put('R23', 468, 543)
put('R27', 815, 675); put('R28', 832, 405)
put('R30', 700, 740, True); put('R32', 715, 757, True)
for k, x in enumerate([315, 380, 450, 515, 590, 655, 725, 795]):             # D3-D10: the row above the keyboard
    put('D%d' % (3 + k), x, 800, True)
put('D11', 118, 463)
OTHER = {'X1': P(815, 740), 'C9': P(155, 555), 'C10': P(468, 435), 'C11': P(555, 705), 'C15': P(800, 718)}
