# Atari 800 (CO14175 Rev A)

`--machine atari800`. The board is simulated at pin level to troubleshoot it, not to run programs.

## What is on the board
- **Motherboard**: POKEY (A101), 6520 PIA (A102), keyboard matrix scanners Z103/Z104, address decoder Z101/Z102, cartridge slots J108 (right) and J109 (left), RAM slots J102-J104, controller ports.
- **CPU board** (J101): 6502 (A303), GTIA (A301), ANTIC (A302), address buffers Z303/Z304, clock gating Z301/Z302.
- **Personality board** (J107): the three OS ROM chips A402 (2K, $D800), A403 (4K, $E000), A401 (4K, $F000), the I/O decoder Z401 and the EXSEL/RASTIME/WRITIME gates Z402/Z403.
- **Three 16K RAM boards**: 4116 DRAMs, 74LS158 row/column multiplexers, 74LS244 data buffer, Z501 gates.

## Timing
Four simulation steps per CPU cycle (1.79 MHz). The OSC divides to F0, Z302 makes PHI2. ANTIC decides one cycle ahead to halt the 6502 for DMA and for its nine refresh cycles a line (a fetch that lands in a refresh slot does the refresh itself). R403 and the RAS delay on each RAM board are one step each.

## What runs
The rev B OS reaches the Memo Pad screen; typing works through the keyboard matrix. Loading the 8K BASIC ROM plugs it into J109 (one ROM part, `CARTL`, wired to the slot; it drives RD5 and takes the RAM's $A000 select away) and the machine reaches `READY`.

## Not simulated
Video, chroma and audio output networks (the picture is drawn by the GTIA model straight to the frame buffer), serial bus, controller ports, DRAM decay and refresh. ANTIC's dummy cycle for a display list instruction without LMS is a known deviation from real timing.

## Uncertain wiring
`tools/atari800_netlist.py` lists 23 items where the print was not legible (run it to see them). Notable: the F0 line into Z302 pin 13, R/W LATE as R/W OR WRITIME (Z102C/D), the RAM select pins per slot. `ATARI800_BEEP_CHECKS.md` has the continuity checks that settle them.

## Headless tests
`tests/atari800/`: `boot.c` (run frames, write a PPM), `keys.c` (type text; `CART=file` plugs a cartridge), `trace.c`, `pins.c`, `who.c`, `dma.c`, `ram.c`, `scan.c`, `fault.c` (open one pin: `/tmp/atari_fault REF PIN`).
