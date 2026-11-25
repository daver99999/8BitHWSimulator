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
- The board's UC1 wiring gives a 12-clock long cycle (908 clocks a line) where the reference testbench expects 16 (912). To be checked on the real board.
