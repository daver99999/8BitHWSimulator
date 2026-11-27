# Atari 800 (motherboard CO14175 Rev A, CPU board, personality board, RAM boards): continuity checks of the netlist

Boards unpowered, meter on beep. The netlist (`tools/atari800_netlist.py`) was typed in from the Atari 400/800 Field Service Manual, Section 2 (May 1983, 600 dpi scan). These are the connections the print did not show clearly; each one is a place where the netlist could be wrong. Everything else was read clean.

Chip pins are numbered as on the chip (pin 1 at the notch, counter-clockwise). Silkscreen names on your boards may differ from the schematic names; say so and the table will follow your board.

## 1. CPU board: where does the line into Z302 pin 13 come from? (measured 2026-11-27)
Result on the owner's board: Z302 (74LS74) pin 13 (CLR2) has no continuity to ANTIC 35, GTIA 29, edge pin X or the +5 V pin (Z302 pin 14), and there is no trace on it on either side. The print puts it on one wire with Z301 pin 6 and the lower end of R310, and that wire is the F0 clock from GTIA pin 29.

Why it matters: the owner reports the Atari works fine with Z302 pin 13 unconnected, so a floating CLR2 is how this board is built (a floating LS input reads high) and my reading of the print is wrong here. The print has Z302 flop 2 (D and CLK on +5 V) working as a buffer that needs CLR2 to follow F0; that cannot be what the real board does. The netlist still keeps the print because it boots; lifting pin 13 in the simulator gives a dead machine (PC stays 0000), which does not match the real board and must be fixed once the real wiring of Z301 pin 6 / Z302 pins 10 and 13 is known.

| To | Result |
|---|---|
| ANTIC (A302) pin 35 | NO beep |
| GTIA (A301) pin 29 | NO beep |
| edge connector pin X (Reset A) | NO beep |
| Z302 pin 14 (+5 V) | NO beep |
| Z301 pin 6 to GTIA (A301) pin 29 | NO beep (so Z301 pin 6 is not on F0 either) |
| R310, end away from +5, to Z301 pin 6 | NO beep (so R310 is not on Z301 pin 6 either) |
| Z301 pin 6 to Z301 pin 7 (GND) | NO beep |
| Z301 pin 4 to Z302 pin 10 | BEEP (g = good) |
| Z301 pin 5 to Z301 pin 7 (GND) | BEEP |
| Z301 pin 6 to Z302 pin 13 | BEEP (owner: pin 6 goes to Z302 pin 13). So pin 13 is joined to Z301 pin 6, but not to F0, R310, +5 V, ground or edge X. What drives this wire is not known yet. |
| Z302 pin 13 to any edge-connector pin | NO beep (owner, 2026-11-27) |
| Z302 pin 13 to Z302 pin 9 | NO beep |
| Z302 pin 13 to Z302 pin 8 | NO beep |

## 2. CPU board: 6502 RDY pin (open)
The print labels two pins "7" (SYNC and RDY). On a real 6502 SYNC is pin 7 and RDY is pin 2. Beep 6502 (A303) pin 2 to ANTIC pin 15 (RDY) and to the lower end of R311.

## 3. Motherboard: POKEY power pins (open)
The drawing shows Vcc on POKEY pin 17 and Vss on pin 11, beside a pin 1 that a real POKEY grounds. With the POKEY out of its socket, beep the socket pin 1 and pin 11 to ground, and pin 17 to +5V.

## 4. Motherboard: RAM slot select lines (open)
J102, J103, J104 pin 18 and pin U each get one of the eight-kilobyte block selects from Z101. Beep Z101 pins 1, 2, 3, 4 to J102 pin 18, J102 pin U, J103 pin 18, J103 pin U. The netlist assumes in that order.

## 5. Motherboard: Z102 gates and the cartridge RD4 / RD5 (open)
Beep Z102 pins 1, 2, 4, 5 (and outputs 3 and 6) to Z101 pin 5 (S4), Z101 pin 6 (S5), J109 pin A (RD4), J109 pin 14 (RD5), J104 pin 18, J104 pin U.

## 6. Motherboard: keyboard connector J106 pins 15 and 10 (open, narrowed)
Reread at 800 dpi; the scan is blurred at two of the eight pins on the Z104 side. The other six (16, 12, 17, 14, 11, 13) read clean, and 15 and 10 are the two left over, so only their order is open. The netlist has 15 at the second position and 10 at the fifth (R120 and R123). Beep J106 pin 15 to the far end of R120; if it beeps to R123 instead, the two are swapped.

## 7. Motherboard: Z101 74LS42 inputs, PIA chip selects, power-on reset, J115 power pins, J101 pin 5 / S3 (open)
Details are listed in the generator output (`python3 tools/atari800_netlist.py`).

## 8. Personality board: ROM chip-select pins and the Ø2B edge pin (open)
Beep A401 / A403 pin 20 to the edge pin for S7 (H) and pin 21 to A12 (pin 12); A402 pin 20 to S6 (M), 21 to A12, 18 to A11 (L). Find which edge pin carries Ø2B (the print shows 5, which is also A0).

## 9. 16K RAM board: which DRAM holds which bit (open)
The netlist puts D6 in Z505, D7 in Z506, D5 in Z507, D3 in Z508, D4 in Z509, D2 in Z510, D1 in Z511, D0 in Z512. Beep DRAM pin 2 (DIN) to the edge pin of its data bit.

## 10. Motherboard: console switch header J114 pin order (open)
Added after rereading the print at higher resolution. The four 100 ohm resistors R184-R187 sit left to right over J114 pins 1, 4, 3, 2, and the four wires from the CPU slot (S2, S1, S0, /RNMI from the top down) nest without crossing, which puts /RNMI on pin 1, S0 on pin 4, S1 on pin 3 and S2 on pin 2. The netlist and the key map currently have S2 on 1, S1 on 2, S0 on 3 and /RNMI on 4. Beep J101 pin R (S2) to J114 pins 1-4 to settle it; the netlist and the key map change if it is not pin 1.

## 11. Video output, motor transistor, serial bus pull-ups (open)
Netted from the print for continuity only (the simulator draws the picture from the GTIA model): the luminance mixer A103B-F, R179-R183, CR108, Q105, R189, R200-R206, C163, C164, C170, L103-L105, the chroma amplifier A104A/B with R196-R199, Q107 with R159 and L106, and the serial bus pull-ups and capacitors R129A-F, R157, R158A, C182-C188, C189. The unsure items in the generator output say what to check first: the order of R180-R183 on the A103 outputs (36K on pin 4, 18K on 10, 9.1K on 15, 4700 on 12), CR108's polarity, the A104B base pin, and which serial bus line each pull-up is on.

## 12. Motherboard: RAM slot select wiring and the gated S4 / S5 (open)
Read at 330 dpi. Z101 Y0 and Y1 go to J104 pins 18 and U (slot 1, so the first 16K board is in J104, and the netlist now has it so), Y2 and Y3 reach J103, and the gated S4 / S5 (Z102A pin 6, Z102B pin 3) go to J107 pins P and N and to J103 pins P and M. Not understood: short wires between J103 pins 18, T, R, N and J104 pins T, S, P, M, and between J102 pins U, 18 and J104 pins N, R. Beep: Z101 pin 1 to J104 pin 18, Z101 pin 2 to J104 pin U, Z101 pins 3 and 4 to every pin of J103 (note which), Z102 pin 6 to J107 pin P and to J103 pin P, Z102 pin 3 to J107 pin N and to J103 pin M, then J102 pins 18 and U to every pin of J103 and J104. The netlist has J103 on Y2/Y3 and J102 on the gated S4/S5 for now, and the personality board's S4 / S5 pins on the raw selects (the print shows the gated ones).

## 13. Motherboard: Z101 pin 12, R103 and the speaker resistor R102 (open)
Read at 330 dpi, changed in the netlist. Z101 pin 12 (D) is on the EXSEL line (J101 pin E), not ground, and R103 680 hangs from that line to ground (it was on S6). The speaker resistor R102 1K is fed from J101's S3 line (the GTIA speaker bit), not from the POKEY audio line. Beep: Z101 pin 12 to J101 pin E and to R103's upper end; R102's J101 end to J101 pin S (S3) and to GTIA pin 15; check that R102 does not beep to POKEY pin 37.

## 14. Cartridge slots J108 / J109: pin S and pin R
The print labels pin S "RAS" and pin R "R/WE" on both slots. The netlist has R on R/W (same as the CPU R/W) and S on phase 2 (the FAQ pinout), and J108 now matches J109 (it was wired to pins 1 and A, which was wrong: A is RD4).
- Beep J109 pin S to J108 pin S (they should be one wire), then J109 pin S to RAM slot J104 pin 14 (phase 2) and to J104 pin 15 (RAS time). Whichever beeps is what pin S really is.
- Beep J109 pin R to J108 pin R, then to J104 pin 16 (R/W).

## 15. Controller ports: pot resistors and POKEY pin 11
Read from the print at 200 dpi and added to the netlist: R130-R137 (1.8K) in series with the pots, R158B-R158I (4.7K) from +5 to PB0-PB7, and the small capacitors on the jack side. Two things are not proved:
- POKEY pin 11: the print has P5 on it and also a ground at the foot of the chip labelled 11. The netlist has P5 on 11 and ground on pin 1 only. Beep A101 pin 11 to ground; if it beeps, that pin really is ground, and P5 (jack 3 B pot) has no pin.
- Pot to jack pairing: beep A101 pin 14 (P0) through R131 to J110 pin 9, and A101 pin 15 (P1) through R130 to J110 pin 5. Jack 2 should be P2 / P3 (pins 12 / 13), jack 3 P4 / P5, jack 4 P6 / P7.

## 16. Personality board: delayed phase 2 and the EXSEL node
The gate wiring was reread and matches the netlist. Two points are only from the drawing:
- Phase 2 delay: beep J107 pin S to Z402 pin 5 and R403 pin 1, then the far end of L403 (C410 side) to Z402 pins 10 and 4. These two groups must not touch each other (R403 and the coils are in between).
- EXSEL node: beep J107 pin 14 to Z403 pins 3, 6, 8, 13, Z402 pin 2 and R402. All of them are one wire.

## 17. Oscillator, RF stage, sound amplifier and J115 power pins (open)
New this pass, read at 300 dpi.
- J115 power pins: the print puts -5 V on pin 1 and +5 V (the 5B supply) on pin 17; I had them the other way round before. Pins 18 and 19 are +5 V (5A), 21 is +12 V, 15, 16 and 22 are ground. Beep J115 pin 1 to the -5 V pin of a RAM slot (20 or X), and pin 17 to +5 V (19 or W).
- Oscillator: X101 pin 1 to R105 and Q102 base; X101 pin 2 to R107 and Q103 collector; Q102 collector to R106 and J101 pin C (OSC). Both transistors are PNP by the arrows on the print, so check the lead order of the real parts by their markings, not by this netlist.
- Sound amplifier: A104 pins 4 / 2 are the two bases, 3 the shared emitter (R164 to ground), 5 and 1 the collectors. J115 pin 2 (audio out) is behind R194 and J115 pin 13 (speaker audio) behind R195.
- RF stage: Q106 collector to L102 and C173; emitter to C169, R188, R190 and C172.

## Not netted
The six supply chokes L107-L112 at J115 (the netlist has the rails go straight to the connector, because the simulator feeds power in there) and the power supply board. The oscillator, RF stage, sound amplifier and the capacitors at J115 are netted from check 17, but the chokes and the oscillator's simulator clock (the part OSCSIM, which is not a real part) are not real continuity.
