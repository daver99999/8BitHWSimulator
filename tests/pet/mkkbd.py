#!/usr/bin/env python3
"""Test ROM for UD6: scan the keyboard (PIA1 PA0-3 -> 74145 row, PB0-7 the columns) forever and store the
ten PB readings at $8040..$8049 (video RAM, read back with pet_vram_peek)."""
import sys
code = bytearray()
def emit(*b): code.extend(b)
base = 0xF000
emit(0x78, 0xD8, 0xA2, 0xFF, 0x9A)
emit(0xA9, 0x00, 0x8D, 0x11, 0xE8)      # CRA = 0: DDRA
emit(0xA9, 0x0F, 0x8D, 0x10, 0xE8)      # DDRA = $0F
emit(0xA9, 0x04, 0x8D, 0x11, 0xE8)      # CRA = 4: port A data
emit(0xA9, 0x04, 0x8D, 0x13, 0xE8)      # CRB = 4: port B data
loop = base + len(code)
for r in range(10):
    emit(0xA9, r, 0x8D, 0x10, 0xE8)     # LDA #r STA $E810
    emit(0xAD, 0x12, 0xE8)              # LDA $E812
    emit(0x8D, 0x40 + r, 0x80)          # STA $8040+r
emit(0x4C, loop & 255, loop >> 8)
rom = bytearray([0xFF] * 4096)
rom[:len(code)] = code
for v in (0xFFA, 0xFFC, 0xFFE): rom[v], rom[v + 1] = 0x00, 0xF0
open(sys.argv[1], 'wb').write(rom)
