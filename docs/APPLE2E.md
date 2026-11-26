# Apple IIe (unenhanced, board 820-0064-B)

Built for troubleshooting the hardware, not for loading programs.

## Sources
- Netlist: Bit Preserve KiCad capture of 050-0051-D (rjhelms / baldengineer "bit-preserve", CC BY 4.0), read by `tools/apple2_sch2net.py`, converted by `tools/apple2e_netlist2c.py`.
- MMU and IOU: generated from frozen-signal's `Apple_IIe_MMU_IOU` VHDL (CC0) by `tools/vhdl2c.py` (`tools/gen_apple2e_chips.sh`).
- Timing HAL (UD1, 16R8): the ABEL equations, ported in `src/apple2e_chips.c`.
- ROMs (not in the repository), in `roms/apple2e`: 342-0135-B (CD, UE8), 342-0134-A (EF, UE10), 342-0132-B (keyboard, UE12), 342-0133-A (video, UF4).

## Behaviour
Boots to the `Apple ][` banner and `]` prompt; the keyboard works through the UE14 encoder. The 80-column card and auxiliary RAM are absent, so the ROM's aux test sees a mismatch at $0400.

## Modelling notes
- The IOU resets low for about 36000 cycles at power-on; RESET is open collector and the model supplies the pull-up.
- Row address RA0..7 = A0-A5, A7, A8; column = A9, A6, A10-A15, with the byte reversed into the 6664's pins.
- Latches on a derived clock (UD3, clocked by PHI0) take their data from the previous commit, as the real edge arrives before the logic it starts moves the data.
- Board view: ICs on the board grid (UE4 = column E, row 4, F at the left); each resistor, capacitor, connector and small part sits in the grid cell the SAMS Computerfacts CC10 "GridTrace location guide" gives it (C1 is A-3, R27 is C-10; table in `tools/apple2e-netlist-src/sams_grid.txt`), as a strip below the chips of its row; the few parts the guide does not list sit under the chip they share most signals with; slots J1-J7 (J0 the aux connector) lie along the right with slot 1 at the top.
- UC1 (74LS109): the KiCad capture has pin 14 on pin 7 and pin 13 on 7M. Beeped on the real board: pin 14 to pin 6 (7M) and pin 13 to pin 7 are connected, pin 14 to 7 is open. The netlist now follows the board. This gives the 16 clock long cycle (912 clocks a line).
