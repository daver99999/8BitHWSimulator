# Atari 800 (motherboard CO14175 Rev A, CPU board, personality board, RAM boards): continuity checks of the netlist

Boards unpowered, meter on beep. The netlist (`tools/atari800_netlist.py`) was typed in from the Atari 400/800 Field Service Manual, Section 2 (May 1983, 600 dpi scan). These are the connections the print did not show clearly; each one is a place where the netlist could be wrong. Everything else was read clean.

Chip pins are numbered as on the chip (pin 1 at the notch, counter-clockwise). Silkscreen names on your boards may differ from the schematic names; say so and the table will follow your board.

## 1. CPU board: where does the line into Z302 pin 13 come from? (open)
Z302 (74LS74) pin 13 (CLR2) is on one wire with Z301 pin 6 and the lower end of R310. The print does not show where that wire goes. Beep Z302 pin 13 to each of these:

| To | Expected | Why it matters |
|---|---|---|
| ANTIC (A302) pin 35 | ? | netlist assumes yes (F0) |
| GTIA (A301) pin 29 | ? | same net as ANTIC 35 |
| edge connector pin X (Reset A) | ? | |
| edge connector pin C (OSC) | ? | |
| edge connector pin 25 (Reset M) | ? | |
| Z301 pin 6 | beeps | same wire |
| R310, end away from +5 | beeps | same wire |

## 2. CPU board: 6502 RDY pin (open)
The print labels two pins "7" (SYNC and RDY). On a real 6502 SYNC is pin 7 and RDY is pin 2. Beep 6502 (A303) pin 2 to ANTIC pin 15 (RDY) and to the lower end of R311.

## 3. Motherboard: POKEY power pins (open)
The drawing shows Vcc on POKEY pin 17 and Vss on pin 11, beside a pin 1 that a real POKEY grounds. With the POKEY out of its socket, beep the socket pin 1 and pin 11 to ground, and pin 17 to +5V.

## 4. Motherboard: RAM slot select lines (open)
J102, J103, J104 pin 18 and pin U each get one of the eight-kilobyte block selects from Z101. Beep Z101 pins 1, 2, 3, 4 to J102 pin 18, J102 pin U, J103 pin 18, J103 pin U. The netlist assumes in that order.

## 5. Motherboard: Z102 gates and the cartridge RD4 / RD5 (open)
Beep Z102 pins 1, 2, 4, 5 (and outputs 3 and 6) to Z101 pin 5 (S4), Z101 pin 6 (S5), J109 pin A (RD4), J109 pin 14 (RD5), J104 pin 18, J104 pin U.

## 6. Motherboard: keyboard connector J106 pins 15, 17, 10, 14 (open)
Beep J106 pins 16, 15, 12, 17, 10, 14, 11, 13 to the far end of R119 through R126 in turn; the netlist expects R119..R126 in that order.

## 7. Motherboard: Z101 74LS42 inputs, PIA chip selects, power-on reset, J115 power pins, J101 pin 5 / S3 (open)
Details are listed in the generator output (`python3 tools/atari800_netlist.py`).

## 8. Personality board: ROM chip-select pins and the Ø2B edge pin (open)
Beep A401 / A403 pin 20 to the edge pin for S7 (H) and pin 21 to A12 (pin 12); A402 pin 20 to S6 (M), 21 to A12, 18 to A11 (L). Find which edge pin carries Ø2B (the print shows 5, which is also A0).

## 9. 16K RAM board: which DRAM holds which bit (open)
The netlist puts D6 in Z505, D7 in Z506, D5 in Z507, D3 in Z508, D4 in Z509, D2 in Z510, D1 in Z511, D0 in Z512. Beep DRAM pin 2 (DIN) to the edge pin of its data bit.

## Not modelled yet
The oscillator and video networks (Q102-Q106), the SIO motor transistor Q107, the power supply board.
