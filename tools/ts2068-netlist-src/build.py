import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sheet01, sheet02
from netlib import N
N.check()
N.dump(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'ts2068_netlist.json'))
