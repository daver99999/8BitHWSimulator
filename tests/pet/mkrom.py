#!/usr/bin/env python3
"""Build a 4K test ROM for socket UD6 ($F000-$FFFF): initialises the CRTC (40 columns), writes HELLO to the
display RAM, tests the main RAM and writes the results into the display RAM, then loops counting."""
import sys
code = bytearray()
def emit(*b): code.extend(b)
base = 0xF000
emit(0x78, 0xD8, 0xA2, 0xFF, 0x9A)              # SEI CLD LDX #$FF TXS
crtc = [0x31, 0x28, 0x29, 0x0F, 0x27, 0x00, 0x19, 0x20, 0x00, 0x07, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00]
for r, v in enumerate(crtc):
    emit(0xA9, r, 0x8D, 0x80, 0xE8)             # LDA #r  STA $E880
    emit(0xA9, v, 0x8D, 0x81, 0xE8)             # LDA #v  STA $E881
for i, ch in enumerate([8, 5, 12, 12, 15]):    # HELLO (screen codes)
    emit(0xA9, ch, 0x8D, i & 255, 0x80)        # LDA #ch STA $8000+i
emit(0xA9, 0x0C, 0x8D, 0x4C, 0xE8)                      # VIA PCR = $0C: CA2 low (GRAPHIC): the first 1K of the character ROM
# main RAM test: write $55 and $AA patterns to $0200 and $4200 (both 16K banks), read back, store at $8010..
n = 0
for addr in (0x0200, 0x4200):
    for v in (0x55, 0xAA):
        emit(0xA9, v, 0x8D, addr & 255, addr >> 8)      # LDA #v STA addr
        emit(0xAD, addr & 255, addr >> 8)               # LDA addr
        emit(0x8D, 0x10 + n, 0x80)                      # STA $8010+n
        n += 1
# read the ROMs back through the bus: $F000 (self), copy the first byte
emit(0xAD, 0x00, 0xF0, 0x8D, 0x20, 0x80)                # LDA $F000 STA $8020
# VIA: write and read back the timer latch (T1 latch low $E846)
emit(0xA9, 0x5A, 0x8D, 0x46, 0xE8, 0xAD, 0x46, 0xE8, 0x8D, 0x21, 0x80)   # LDA #$5A STA $E846 LDA $E846 STA $8021
# PIA1: set DDRA ($E811 with CRA bit2 clear) = $0F, read back
emit(0xA9, 0x00, 0x8D, 0x11, 0xE8)                      # CRA = 0 (select DDR)
emit(0xA9, 0x0F, 0x8D, 0x10, 0xE8)                      # DDRA = $0F
emit(0xAD, 0x10, 0xE8, 0x8D, 0x22, 0x80)                # LDA DDRA STA $8022
loop = base + len(code)
emit(0xEE, 0x30, 0x80)                                  # INC $8030 (heartbeat)
emit(0x4C, loop & 255, loop >> 8)                       # JMP loop
rom = bytearray([0xFF] * 4096)
rom[:len(code)] = code
rom[0xFFA], rom[0xFFB] = 0x00, 0xF0                     # NMI
rom[0xFFC], rom[0xFFD] = 0x00, 0xF0                     # RESET
rom[0xFFE], rom[0xFFF] = 0x00, 0xF0                     # IRQ
open(sys.argv[1], 'wb').write(rom)
