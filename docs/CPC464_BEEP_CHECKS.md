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
1. IC111 pin 29 to IC103 pin 14 (both ground): G (owner first said N, corrected: wrong IC probed).
2. J104 pin 3 to IC111 pin 29: G.
3. IC111 pin 29 to IC116 pin 35: G (settles the gate array ground pin).
4. IC111 pin 29 to IC107 pin 7: G.
5. IC111 pin 29 to IC108 pin 1: G.
6. IC111 pin 29 to IC102 pin 6: G. (Remaining grounds skipped, owner asked, low value.)
7. IC112 pin 11 to IC106 pin 11: G (retest; first N was a slip). Matches the netlist.
8. IC112 pin 11 to IC106 pin 14: owner said it beeps to IC106 pin 20 (slip, SUSPECT; probe was on the +5V pin).
9. IC106 pin 20 to IC112 pin 14: G (both +5V).
10. IC112 pin 11 to IC112 pin 14: first G, retest (18) N. First answer was a slip; IC112 pin 11 is NOT on +5V. Fault finding withdrawn.
11. IC112 pin 8 to IC112 pin 14: N (other gate output is not on +5V).
12. IC112 pin 11 to IC106 pin 20: G (both +5V, no new information).
13. IC106 pin 11 to IC106 pin 20: N (latch clock is not on +5V). Consistent with 18.
14. IC106 pin 11 to IC112 pin 8: N (twice).
15. IC106 pin 11 to IC112 pin 3: N.
16. IC106 pin 11 to IC112 pin 11: G (owner: first answer was a slip). Matches the netlist; supersedes beep 7 (N, SUSPECT).
18. IC112 pin 11 to IC112 pin 14: N (retest of 10). Conflict resolved: IC112 pin 11 / IC106 pin 11 is the printer latch clock net, not on +5V. Open reading question 1 is closed: the clock is IC106 pin 11.
19. IC106 pin 12 to pin 16: N (tracer misread confirmed, they are not joined).
20. IC107 pin 19 to IC107 pin 7 (LK1): N, not fitted.
21. IC107 pin 20 to pin 7 (LK2): N, not fitted.
22. IC107 pin 21 to pin 7 (LK3): N, not fitted.
23. IC107 pin 22 to pin 7 (LK4): N, not fitted. (All four links open; whether the pads exist on his board is not checked.)

OPEN (superseded, see 16-18): where does IC106 pin 11 go? Not IC112 pin 11 (N), not +5V (N).
Next beeps, one per message: IC106 pin 11 to IC112 pin 8; then to IC112 pin 3; then IC112 pin 12 / 13 to find which gate really drives it.
Also: the tracer joined IC106 pins 12 and 16 (PRQ4 / PRQ6) on the drawing. Probably a crossing on the drawing, beep IC106 pin 12 to pin 16 to confirm they are NOT joined.
The tracer does not see IC112 (gates are drawn as symbols, not boxes), so its gate pins are checked by eye only.

ROM image supplied by the owner (cpc464.rom, 32768 bytes, md5 a993f85b88ac4350cf4d41554e87fe4f): reads as the 464 firmware v1 (1984 Amstrad copyright string). Kept outside the repo; not for loading programs, only to compare against IC103 if needed.
