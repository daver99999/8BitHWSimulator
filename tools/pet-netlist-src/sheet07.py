from netlib import N
from config import COLS
N.sheet_is('07')
# Display RAM address mux: SAn = (CLK1B ? TAn : BAn) in 40-col jumper position (40C: SAn=BAn; 80C would be BA(n+1))
N.chip('UC8','LS157'); N.chip('UC9','LS157'); N.chip('UC10','LS157')
for ref in ('UC8','UC9','UC10'):
    N.pins(ref, {1:'CLK1B', 15:'GND'})
# UC8: only 3 and 4 used (SA0, SA1); 1,2 unused
def B(n): return 'BA%d' % (n if COLS==40 else n+1)   # link matrix: 40C SAn=BAn, 80C SAn=BA(n+1)
N.pins('UC8', {11:'TA0', 10:B(0), 9:'SA0', 14:'TA1', 13:B(1), 12:'SA1'})
N.pins('UC9', {2:'TA2', 3:B(2), 4:'SA2', 5:'TA3', 6:B(3), 7:'SA3',
               11:'TA4', 10:B(4), 9:'SA4', 14:'TA5', 13:B(5), 12:'SA5'})
N.pins('UC10',{2:'TA6', 3:B(6), 4:'SA6', 5:'TA7', 6:B(7), 7:'SA7',
               11:'TA8', 10:B(8), 9:'SA8', 14:'TA9', 13:B(9), 12:'SA9'})
N.note('Jumper matrix follows config.COLS (user board photo: wires beside BA1..BA10 = 80C, SAn=BA(n+1)). Select net is drawn CLK1B - check name on sheet 6 (may be CLK1S).')
