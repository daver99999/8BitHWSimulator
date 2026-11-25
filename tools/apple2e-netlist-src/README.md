# Apple IIe (820-0064-B, unenhanced) netlist source

`apple2e.net` is traced from the KiCad schematic of the Apple IIe (NTSC) drawing 050-0051-D, captured by rjhelms for
the Bit Preserve project (https://github.com/baldengineer/bit-preserve, Apple/Apple IIe), licence CC BY 4.0. It was made with

    python3 tools/apple2_sch2net.py "<bit-preserve>/Apple/Apple IIe/Apple IIe (NTSC) - 050-0051-D" tools/apple2e-netlist-src/apple2e.net

after pointing the tool's top sheet name at `Apple IIe (NTSC) - 050-0051-D.kicad_sch`. The parts match the Sams service manual for
the A2S2064 (6502A, MMU UE4, IOU UE5, 2365/2764 ROMs UE8 and UE10, keyboard encoder UE14), which is the unenhanced IIe.

The converter warns about 18 pins that land on two nets (UB8, UC1, UC5, UE1 and UF4): the multi-gate parts that appear on
several sheets. They are being checked against the Sams schematic (pages 53 to 60 of CC10) before the netlist is used.
