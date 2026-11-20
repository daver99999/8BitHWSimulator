"""Where the parts are on the ZX81/TS1000 Issue 1 board, read from five photos of a US Issue 1 board (the first, a whole-board shot,
turned a quarter turn so the edge connector is at the top, like the Issue 3's drawing). The photo shows a composite video mod
in place of the modulator, a 32K RAM chip in place of the 2114s and a switching regulator: none is drawn (the modulator pins are
drawn where its pads are). Chips are drawn pin 1 at the top left. The scale (14.2 px per mm in the turned photo, from the 40 pin
sockets and RP1) is good to a few percent: the photo is taken at an angle, so positions are good to a few mm.
Read from the silkscreen: R18-R22, R23-R26, RP1, RP3, R15-R17, R5, R6, D1-D8 (D1 at the left), the jacks and connectors. The names of the small
resistors round the jacks and the regulator, and of C4/C6/C7, are guesses from their colour bands and the circuit."""
ORIGIN = (0, 0)
SCALE = 4.0
X0, Y0, PXMM = 375.0, 315.0, 14.2
SIZE = (round((2466 - X0) / PXMM * SCALE), round((1755 - Y0) / PXMM * SCALE))

def P(x, y):
    return (round((x - X0) / PXMM * SCALE), round((y - Y0) / PXMM * SCALE))

IC = {'IC1': P(975, 789), 'IC3': P(1581, 1059), 'IC2': P(1239, 1434), 'IC4': P(2199, 1029)}
CONN = {
    'J1': P(1971, 375) + ('h', 2),
    'KB1': P(1971, 1665) + ('h', 1), 'KB2': P(2277, 1671) + ('h', 1),
    'EARJ': P(465, 1065) + ('v', 1), 'MICJ': P(465, 1275) + ('v', 1), 'DC': P(465, 1491) + ('v', 1),
    'MOD': P(645, 900) + ('h', 1),
}
SIP = {                       # a resistor pack: centre, orientation of its row, pin 1 end (the common): 'b' bottom / 'r' right
    'RP1': P(1425, 1065) + ('v', 'b'),
    'RP3': P(1992, 1578) + ('h', 'r'),
}
TWO_PAD = {}
def put(ref, x, y, horizontal=False):
    px, py = P(x, y)
    TWO_PAD[ref] = (px, py, 'h') if horizontal else (px, py)

for k, ref in enumerate(['R18', 'R19', 'R20', 'R21', 'R22']): put(ref, 1830 + 66 * k, 1374, True)
for k, ref in enumerate(['R23', 'R24', 'R25', 'R26']): put(ref, 1881 + 66 * k, 1464, True)
for k, ref in enumerate(['R7', 'R8', 'R9', 'R10']): put(ref, 1195, 818 + 76 * k, True)      # the box of eight 470 ohm: two columns of four
for k, ref in enumerate(['R11', 'R12', 'R13', 'R14']): put(ref, 1313, 860 + 75 * k)
put('R15', 1794, 706); put('R16', 1900, 714); put('R17', 1974, 706)
put('R5', 1318, 693, True); put('R6', 1325, 738, True)
put('R1', 930, 344, True); put('C1', 941, 373, True); put('R2', 1122, 455); put('R4', 1135, 505)
put('C2', 1270, 440); put('C4', 1255, 540); put('C6', 1120, 740); put('C7', 1215, 770)
put('R29', 654, 1126, True); put('R28', 686, 1354, True); put('R27', 680, 1406, True)
put('R30', 894, 1380, True); put('R34', 894, 1426, True); put('R33', 760, 1066)
put('R32', 900, 1200, True); put('R31', 940, 1250, True); put('D9', 760, 1180)
put('C10', 620, 1140); put('C11', 740, 1274); put('C12', 700, 1190)
put('C8', 1569, 1599); put('C9', 2256, 1308)
for k in range(8): put('D%d' % (1 + k), 2145 + 33 * k, 1581)
OTHER = {'X1': P(1190, 675), 'TR1': P(1091, 350), 'TR2': P(1300, 615), 'C5': P(1305, 380), 'C3': P(1335, 520)}
