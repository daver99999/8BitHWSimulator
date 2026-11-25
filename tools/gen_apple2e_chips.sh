#!/bin/sh
# gen_apple2e_chips.sh - regenerate src/apple2e_mmu.inc and src/apple2e_iou.inc, the C models of the Apple IIe's MMU (344-0010)
# and IOU (344-0020), from frozen-signal's VHDL re-implementation of them (CC0, https://github.com/frozen-signal/Apple_IIe_MMU_IOU,
# redrawn from Apple's schematics). Usage: tools/gen_apple2e_chips.sh /path/to/Apple_IIe_MMU_IOU   (a checkout of that repository)
set -e
R="${1:?path to the Apple_IIe_MMU_IOU checkout}"
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/../src"
NOP="$R/CUSTOM/MMU_HOLD_TIME/NOP/MMU_HOLD_TIME.vhdl $R/CUSTOM/DELAY_OSCILLATOR/NOP/DELAY_OSCILLATOR.vhdl $R/CUSTOM/DRAM_HOLD_TIME/NOP/DRAM_HOLD_TIME.vhdl"
python3 "$HERE/vhdl2c.py" MMU mmu "$OUT/apple2e_mmu.inc" $R/MMU/*.vhdl $R/COMMON/*.vhdl $R/TTL/*.vhdl $NOP
python3 "$HERE/vhdl2c.py" IOU iou "$OUT/apple2e_iou.inc" $R/IOU/*.vhdl $R/COMMON/*.vhdl $R/TTL/*.vhdl $R/CUSTOM/POWER_ON_DETECTION.vhdl $NOP
