# Commodore PET (8032080 board)

This page is the detail behind the "Commodore PET" section of the README: what is
modelled, what you need to supply, how it was checked, and where it is weaker
than the real board.

> **Who made it.** The PET boards were written by Claude (Anthropic's AI model,
> in Claude Code), directed and tested by David Green
> ([daver99999](https://github.com/daver99999)) against a real PET 8032080-03
> board. The simulator itself is
> [antirez](https://github.com/antirez)'s
> [c64-hw-simulator](https://github.com/antirez/c64-hw-simulator), extended by
> zzleezz in [8bit-hw-simulator](https://github.com/zzleezz/8bit-hw-simulator).
> Where the model had to assume something about the hardware, the assumption
> is written down below. Check a diagnosis against the real machine before
> acting on it.

## The boards

One PCB, two builds:

| In the program | Board | Columns | Keyboard | Editor ROM |
|---|---|---|---|---|
| `PET` > `4032 (40 col)`, `--board 8032080-40` | 8032080-03, Commodore PET 4032 | 40 | graphics | 901499-01 |
| `PET` > `8032 (80 col)`, `--board 8032080-80` | 8032080, Commodore PET 8032 | 80 | business | 901474-03 |

The 80 column build adds the odd display RAMs (UC6, UC7), their latch (UB8) and
buffers (UB6, UB7), and its character ROM address path differs: the two latches
UB3 (even characters) and UB8 (odd) take turns on the video data bus. The 40
column build has the even set only (UC4, UC5, UB3, with UB3's output enable
grounded). Both are in the same program.

### What is modelled

At the pins, with the same engine as the C64 and the Spectrum:

- CPU: 6502 (UB14), floooh's core.
- Peripherals: two 6520 PIAs, UB12 (keyboard matrix and cassette) and UB16
  (IEEE-488); the 6522 VIA, UB15 (user port, IEEE handshake, sound by the shift
  register, retrace flag); the 6545 CRT controller, UB13.
- Memory: sixteen 4116 DRAMs, UA4 to UA19, one data bit each (UA4/UA5 are D7 and
  UA18/UA19 are D0; row address BA0 to BA6, column address BA13 then BA7 to
  BA12), the 2114 video RAMs (UC4 and UC5, and on the 80 column build UC6 and UC7
  too), the seven ROM sockets UD6 to UD12 and the
  2316 character ROM (UA3).
- Glue: the 74xx logic (decoders, latches, bus buffers, shift register, the
  74177 clock divider chain), the LM555, the MC3446 IEEE-488 bus buffers
  (inverting, both directions) and the 7417/7425 gates.
- Video: the CRTC's address lines, the character ROM and the video shift
  register produce the picture the monitor would see, as an 800 by 400 raster
  (the 40 column dots are shown twice as wide).
- Power: the +12V, -5V and +9V UNREG supplies from the power connector, the two
  bridge rectifiers CR2 (negative supply for the 7905) and CR4 (raw +16V for the
  7812), the regulators VR1 to VR4 and the protection diodes CR1, CR5, CR6 and
  CR7. A bridge or regulator that is out, or a diode that is open, takes away
  the rails behind it, as on the board.
- Keyboard: the matrix as the editor ROM reads it. The 8032's business
  keyboard table was taken from the editor ROM (901474-03, table at offset $06D1
  of the image); the keypad keys ($B0 to $B9) are the ones the editor does not
  shift. The graphics keyboard of the 4032 comes from the PET keyboard matrix
  documentation (see "Sources", and the assumption below).
- Reset: a button that shorts the 6502's /RES (UB14 pin 40) to GND, like a
  switch across the pin.

### The ROMs

They are Commodore's and are **not** in this repository. They can be found on
[Zimmers.net](https://www.zimmers.net/anonftp/pub/cbm/firmware/computers/pet/index.html), the Commodore firmware archive
(`/pub/cbm/firmware/computers/pet/`), under the names in the table below. Put the
images in `roms/pet/` (make the folders next to the program), `roms/`, or the
folder the program starts from, and keep the file names: they are found by the
part number in the name. They are fitted when a board is made, so after adding
one restart the program, or open the file from the file browser (Ctrl+O), which
fits it in its socket and starts the machine over:

| Socket | ROM | Part number found in the name |
|---|---|---|
| UD6 | KERNAL 4 | `901465-22` (or `kernal-4`) |
| UD7 | screen editor, 40 column | `901499-01` (or `edit-4-40`) |
| UD7 | screen editor, 80 column | `901474-03` (or `edit-4-80`) |
| UD8 | BASIC 4 $D000 | `901465-21` |
| UD9 | BASIC 4 $C000 | `901465-20` |
| UD10 | BASIC 4 $B000 | `901465-23` |
| UD11, UD12 | expansion ROMs | not named; use `--rom UD11=FILE` |
| UA3 | character set | `901447-10` (or `characters`) |

A name that matches none of these is left alone. The 4032 takes the 40 column
editor and the 8032 the 80 column one, so both can be in the folder. 4K images
go in as they are, a 2K image (the editor) is mirrored in its 4K socket. If you
open a ROM image from the file browser it goes in its socket and the machine
starts over. `--rom UD6=FILE` does the same on the command line (a 2K or 4K
file for UD6 to UD12, 2K for UA3).

Until the KERNAL and the character ROM are there, the status line says
"no KERNAL ROM" or "no character ROM" and the screen stays blank: the CPU has
nothing to run.

### Booting and programs

With the ROMs in place the screen shows `*** COMMODORE BASIC 4.0 ***` and
`31743 BYTES FREE` (the stock 32K) after about 3 seconds of machine time. How
long that takes on your PC depends on its speed, like the other boards (see
"Limits"); the Warp box runs as fast as the PC can.

`.prg` and `.pet` files (two byte load address, then the program) are written
into the 4116 chips as a freezer would, so they load on a damaged board too. A
program at $0401 is BASIC: the pointers at $2A to $2F are set and RUN is typed
into the keyboard buffer. A program elsewhere gets `SYS addr`. Loading while the
machine is still starting waits until BASIC is up, as it does on the C64.

Keys: type as usual; Esc is STOP, End is RVS, Home and Del are HOME and CLR. The
screen text can be read headless with `--headless FRAMES --text`.

## How the board was made

There is no KiCad project for this board. Everything was made from two
Commodore documents, which are **not** included:

- the logic diagram **8032081 rev E** (11 sheets): the netlist was transcribed
  by hand, sheet by sheet, in `tools/pet-netlist-src/sheet01.py` to
  `sheet11.py` (`sheet09.py` is the 80 column only part), with each chip's pins
  from `pinlib.py` and a check for pins connected twice or not at all
  (`netlib.py`);
- the PCB assembly drawing **8032080 rev J, sheet 7**: where each part sits, read
  off the scan (`tools/pet-netlist-src/placement.py`).

To change something, edit the sheet file and rebuild:

    cd tools/pet-netlist-src
    # set COLS = 80 in config.py, then:
    python3 build.py        # writes pet_80col_netlist.json
    # set COLS = 40, then:
    python3 build.py        # writes pet_40col_netlist.json
    cd ../..
    python3 tools/pet_netlist2c.py     # rewrites src/pet4032_board_data.c and src/pet8032_board_data.c

(Delete `tools/pet-netlist-src/__pycache__` between the two builds if Python
picks up a stale `config`.) `crop.py` is a helper to cut a region out of a
schematic scan at full size.

The chip placement is in scan pixels, converted to millimetres at 3.8 pixels per
mm from the board's corner; the chips are all notch-up as drawn. Parts that
`placement.py` does not list are put in a row under the board.

## What was checked

Against the real 8032080-03 board (by its owner, with a meter): the terminal
order of the bridges CR2 and CR4 (the two DC terminals are 1 and 4, the AC ones 2
and 3), that CR2's negative output feeds the middle pin of the 7905 (its input;
the left pin is ground and the right one the output), and that pin 12 of the
connectors J2 and J8 is ground. Against the
logic diagram, a second reading of the uncertain parts: the keyboard connector
and the KIN0 to KIN7 order, the PIA pairing of the IEEE-488 lines, the CA1, CA2,
CB1 and CB2 assignments and the cassette lines, the chip select polarity of the
2316 character ROM, UE11 pin 1 and the MC3446's inversion. Against the ROMs: the
80 column keyboard matrix, and that both builds boot to the BASIC prompt and run
a BASIC program and a SYS routine. The chip cores have unit tests
(`sh tests/chips/run.sh`). The programs in `tests/pet/` boot a board headless and
check keys and nets; they need your ROMs and print their own usage.

## Assumptions and limits

- **The netlist is hand transcribed from scans.** The program checks it for
  pins that join nothing, but a wrong connection that still makes the machine
  work would not be caught. Report any you find on your board.
- **Which 4116 bank is which** (the even references UA4, UA6... hold the upper
  16K, CAS1, the odd the lower) was assumed from the drawing; the machine and
  programs work, and a wrong choice would only change which chip a RAM fault
  lands on.
- **The graphics keyboard matrix** (4032) comes from a published description of
  the PET matrix, which is said to be shared by the 2001N's graphics keyboard; it
  was not checked against the 901499-01 editor ROM. The business keyboard
  (8032) was read from the editor ROM.
- **IEEE-488 and cassette**: the PIA/VIA lines and the MC3446 buffers are
  modelled to the connector, but nothing is on the bus: no disk or printer, no
  tape unit.
- **No daughterboard**: the 40/80 column switcher and the VisiCalc dongle that go
  in the J pads are not modelled. The jumpers of the 40 and 80 column builds are
  fixed by the board you pick.
- **No fault atlas, no screenshot diagnosis**: those cover the C64 and the
  Spectrum. On the PET the Diagnose section says so. Faults can still be put on
  any pin and part by hand, and the continuity test (Shift-click a pad) follows
  the netlist.
- **The picture on the board** puts the chips where the drawing has them, but the
  connectors are simplified (the real edge connectors have fingers on both sides
  of the board; here they are one or two rows of pads), the passives in the
  resistor cluster near the video and IEEE parts are placed to a few
  millimetres, and the outline is a plain rectangle, not the notched shape.
- Speed: like the others, it simulates the board at a fraction of real speed.

## Sources and thanks

- [antirez](https://github.com/antirez), Salvatore Sanfilippo: the simulator
  ([c64-hw-simulator](https://github.com/antirez/c64-hw-simulator)).
- zzleezz: [8bit-hw-simulator](https://github.com/zzleezz/8bit-hw-simulator),
  which this is built on.
- Andre Weissflog (floooh): the 6502 core and the style of the other cores.
- The PET keyboard matrix of the graphics keyboard:
  [masswerk.at, "PET keys, 2001 edition"](https://www.masswerk.at/nowgobang/2023/pet-keys-2001-edition).
- Commodore Business Machines: the logic diagram 8032081 and the PCB assembly
  drawing 8032080, the ROMs and the machine.
- [Zimmers.net](https://www.zimmers.net/anonftp/pub/cbm/firmware/computers/pet/index.html): the archive the PET ROM images come from.
- Claude (Anthropic): the PET netlist, the 6520, 6522 and 6545 cores, the board
  models, video, keyboards, power section, loading, layout, tests and this text.

## Chiclet keyboard (option)

The 4032 can show the 2001's chiclet keyboard instead of the graphics one: `--option keyboard=chiclet` (or the Keyboard choice in the
control panel). It has the same matrix as the graphics keyboard (as confirmed by the board's owner), so only the Keyboard window
changes: the key places and legends come from a photo of a chiclet keyboard (a Japanese one: it has katakana legends, which are not
drawn). The 8032 keeps its business keyboard.
