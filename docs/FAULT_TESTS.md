# Fault tests: CPC 464 and Atari 800

Each row is one fault injected at power on (`tests/faultrun.c MACHINE FRAMES OUT.ppm SPEC`, specs as the simulator's `--fault`), run for the frames shown.
The CPU's PC after the run and whether the picture has anything on it are listed. The first row is the good board.
What this checks is that the failures are plausible for the part (a dead CPU, ROM, gate array or clock gives a dead board, the keyboard chips do not stop the boot...),
not that the simulator matches a real faulty board; compare against a real one when you have one.

## Amstrad CPC 464 (140 frames from power on)

| Fault | CPU PC | Picture |
|---|---|---|
| `IC117:ok` | 1A54 | drawn, 208624 px |
| `IC117:removed` | 0DD2 | blank |
| `IC124:removed` | B93B | blank |
| `IC117.2:open` | B9CE | blank |
| `IC117.14:open` | 0DD2 | blank |
| `IC117.4:open` | 4FBE | blank |
| `IC117.15:open` | 0DD2 | blank |
| `IC117.5:open` | 002C | blank |
| `IC117.10:gnd` | 08A7 | blank |
| `IC117.3:open` | 0C1A | blank |
| `IC103:removed` | 00FA | blank |
| `IC103.11:open` | E61C | blank |
| `IC103.10:open` | 21C3 | blank |
| `IC103.22:open` | 0036 | blank |
| `IC111:removed` | 0000 | blank |
| `IC111.8:open` | BDB0 | blank |
| `IC111.34:open` | 0119 | blank |
| `IC111.6:open` | 0000 | blank |
| `IC116:removed` | 0000 | blank |
| `IC108:removed` | 1A4A | blank |
| `IC107:removed` | 227B | drawn, 208670 px |
| `IC102:removed` | 1D16 | drawn, 208624 px |
| `IC114:removed` | 237A | drawn, 14 px |
| `IC115:removed` | B944 | drawn, 194914 px |
| `IC106:removed` | 1A54 | drawn, 208624 px |
| `IC104:removed` | 1A0C | blank |
| `IC105:removed` | 0627 | blank |
| `IC109:removed` | 0629 | blank |
| `IC113:removed` | 089E | blank |
| `IC101:removed` | 1A54 | drawn, 208624 px |
| `IC112:removed` | F223 | blank |
| `IC110:removed` | 0000 | blank |
| `IC125:removed` | 0000 | blank |

## Atari 800 (70 frames from power on)

| Fault | CPU PC | Picture |
|---|---|---|
| `A303:ok` | EC8F | drawn, 61440 px |
| `A303:removed` | 0000 | blank |
| `A303.33:open` | E806 | drawn, 232 px |
| `A303.12:open` | 0001 | blank |
| `A301:removed` | 0000 | blank |
| `A302:removed` | 0000 | blank |
| `A101:removed` | EA81 | drawn, 61440 px |
| `A102:removed` | EC8F | drawn, 61440 px |
| `A401:removed` | B5B7 | blank |
| `A402:removed` | EC8F | drawn, 61440 px |
| `A403:removed` | E417 | blank |
| `Z505_1:removed` | EBD0 | blank |
| `Z506_1:removed` | 8082 | blank |
| `Z505_1.2:open` | E6F6 | blank |
| `Z505_1.14:open` | EBD0 | blank |
| `Z505_1.4:open` | EBD0 | blank |
| `Z505_1.15:open` | EBD0 | blank |
| `Z505_1.5:open` | 000C | blank |
| `Z101:removed` | D101 | blank |
| `Z102:removed` | 3A6E | blank |
| `Z301:removed` | 0000 | blank |
| `Z302:removed` | 0000 | drawn, 59472 px |
| `Z303:removed` | F221 | blank |
| `Z304:removed` | 343A | blank |
| `Z401:removed` | 0605 | drawn, 53027 px |
| `Z402:removed` | 0000 | blank |
| `Z403:removed` | EC8F | drawn, 61440 px |
| `Z501_1:removed` | 1F54 | blank |
| `Z502_1:removed` | 1F54 | blank |
| `Z503_1:removed` | F142 | blank |
| `Z504_1:removed` | 3970 | blank |
| `A103:removed` | 0000 | blank |
| `OSCSIM:removed` | 0000 | blank |
| `Z103:removed` | EC8F | drawn, 61440 px |
| `Z104:removed` | EC8F | drawn, 61440 px |

Notes
- CPC IC125 (the oscillator) removed: found by this test to leave the board running; the gate array now counts only edges that reach its XTAL pin, so the board stops.
- CPC IC107 removed: BASIC fills with phantom keystrokes ("Syntax error"); IC115 removed: colour garbage; IC114 removed: black screen with specks.
- Atari A402 is the 2K floating-point ROM at $D800 and is not used by the Memo Pad boot, so the boot is the same without it.
- Atari A103 (CD4050) removed or the oscillator removed: no CPU clock, dead board.

## CPC 464 printer port (`tests/cpc464/printer.c`)

BUSY (IC107 pin 24) is held low, as a ready printer would, then `PRINT #8,"A"` is typed and the byte on PRN pins 2-8 is read at each falling edge of /STROBE (PRN pin 1).
The ROM sends CR LF once before the first character after a reset; then `A`, CR, LF.

| Fault | Bytes at the strobes |
|---|---|
| none | 0D 0A 41 0D 0A |
| `IC106.2:open` (Q0, PRN pin 2) | 0C 0A 40 0C 0A (bit 0 lost) |
| `IC106.16:gnd` (Q6, PRN pin 8) | 0D 0A 01 0D 0A (bit 6 lost) |
| `IC112.11:open` (the latch clock) | no strobe, PRCK stays low |
| `IC110:removed` (the strobe gate) | no strobe, PRSTRB stays low |
| `IC106:removed` | no strobe |

