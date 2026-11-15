#!/usr/bin/env python3
"""A tiny test character ROM for UA3 (2316, 2K): drawn by hand for the test program, not the PET's own set.
Screen codes: 8=H 5=E 12=L 15=O, 0x66 = checkerboard block. Everything else is blank."""
import sys
G = {
 8:  ["11000110","11000110","11000110","11111110","11000110","11000110","11000110","00000000"],
 5:  ["11111110","11000000","11000000","11111000","11000000","11000000","11111110","00000000"],
 12: ["11000000","11000000","11000000","11000000","11000000","11000000","11111110","00000000"],
 15: ["01111100","11000110","11000110","11000110","11000110","11000110","01111100","00000000"],
 0x66: ["10101010","01010101","10101010","01010101","10101010","01010101","10101010","01010101"],
}
rom = bytearray(2048)
for code, rows in G.items():
    for r, bits in enumerate(rows):
        rom[code * 8 + r] = int(bits, 2)
open(sys.argv[1], 'wb').write(rom)
