# Amstrad CPC 464 (PCB Z70100): netlist and beep checks

Source: the CPC 464 circuit diagram (redraw after the Amstrad service manual, V1.04 2023-11-16, cpcwiki.eu). The netlist generator is
`tools/cpc464_netlist.py` (writes `tools/cpc464-netlist-src/cpc464.net`). The wire connections were traced from the vector lines of the PDF
and every chip pin to chip pin link was checked against the typed netlist (80 clusters agree; the 5 flagged differences are pin-number
words on the drawing that sit between two pins, not wiring differences).

Purpose: troubleshoot the owner's hardware. The boards are unpowered for beeps. One check at a time, chip numbers, reply G (beep) or N (no continuity).

## Chip numbers (board = schematic)
IC111 Z80A (owner: heat-sunk chip, marking not readable), IC116 gate array 40007 (owner: unmarked, heat-sunk), IC103 ROM (40009, 28 pin),
IC107 8255, IC102 AY-3-8912, IC108 HD6845, IC101 74LS145, IC104 / IC105 / IC109 / IC113 74LS153, IC106 74LS273, IC110 74LS132, IC112 74LS32,
IC114 74LS373, IC115 74LS244, IC117-IC124 4164 DRAM (data bit D7 = IC124 down to D0 = IC120), IC125 ZN7400E.
Connectors: J101 video DIN, J102 joystick, J103 stereo jack, J104 DC jack, CP001 tape (A-H), CP002 keyboard (20 way), edge connectors for printer and expansion.

## Open reading questions (each is a beep check)
1. IC106 clock pin: the drawing numbers it 14 (a 74LS273 has CP on pin 11). Beep IC112 pin 11 to IC106 pin 11 (expected G) and to IC106 pin 14 (expected N).
2. LK1 to LK4 (OS links): which are fitted on the owner board.
3. Transistor references: the drawing says Q101 (motor), Q102 (luminance), Q103 (clock driver); the board silk screen says TR1 to TR3.
4. Gate array pin 12 (ROMEN) to IC103 pin 20 (/CS) and to expansion pin 42.
5. The 56 ohm resistor beside X101 has no reference on the drawing (called RX1 in the netlist).
6. Decoupling caps C105 to C128 are netted straight across +5 V and ground (only the total is on the drawing).
7. J104 jack pin 2 (switch) is not connected.

## Beep list (not started)
1. IC111 pin 29 to IC103 pin 14 (both ground): N (no continuity). Unexpected, netlist says GND. Next: test each to the DC jack ground separately.

ROM image supplied by the owner (cpc464.rom, 32768 bytes, md5 a993f85b88ac4350cf4d41554e87fe4f): reads as the 464 firmware v1 (1984 Amstrad copyright string). Kept outside the repo; not for loading programs, only to compare against IC103 if needed.
