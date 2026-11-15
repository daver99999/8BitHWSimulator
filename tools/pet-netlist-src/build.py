import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from config import COLS
import sheet01, sheet02, sheet03, sheet04, sheet05, sheet06, sheet07, sheet08, sheet10, sheet11
if COLS==80: import sheet09
from netlib import N
N.check()
N.dump(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pet_%dcol_netlist.json' % COLS))
