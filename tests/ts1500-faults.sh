#!/bin/sh
# tests/ts1500-faults.sh - TS1500 faults and what the monitor should show. The EXPECTED column is written from the circuit
# (the schematic and how the ZX81 ROM behaves), not from a real board: these faults are not all possible to make on the real
# machine, so none of them is "checked on a real board" unless a line says so. The simulator has no code that knows about
# any of these symptoms.
#
#   tests/ts1500-faults.sh [outdir]
OUT=${1:-/tmp/ts1500-faults}
BIN=${BIN:-./8BitHWSimulator}
FRAMES=${FRAMES:-600}
mkdir -p "$OUT"

run() {     # name "expected" faults...
    name=$1; expected=$2; shift 2
    args=""
    for f in "$@"; do args="$args --fault $f"; done
    {
        echo "EXPECTED: $expected"
        $BIN --machine ts1500 --headless $FRAMES --text --shot "$OUT/$name.png" $args
    } > "$OUT/$name.txt" 2>&1 &
}

run healthy "a white screen with the K cursor at the bottom left"
run no_u5 "no gate array: no clock, no picture (a dark screen)" U5:removed
run no_xtal "no 6.5 MHz clock: no picture, the CPU stopped" X1:open
run no_cpu "no CPU: no K cursor, a blank picture" U7:removed
run cpu_clk_open "the Z80 gets no clock (U5 pin 58 open): no K cursor, a blank picture" U5.58:open
run no_rom "no ROM: the CPU reads garbage, no K cursor" U2:removed
run rom_cs_open "the ROM is never selected (R25 open): no K cursor" R25:open
run rom_a8_open "ROM address line A8 open (R28): wrong ROM bytes, a crash, no K cursor" R28:open
run no_u3 "the ROM address buffer is gone: wrong ROM bytes, no K cursor" U3:removed
run no_u6 "the data bus buffer is gone: a crash, no K cursor" U6:removed
run ram_bit2 "RAM bit 2 gone (U8): a crash or garbage, no K cursor" U8:removed
run ram_bit5 "RAM bit 5 gone (U15, the schematic has it as U13): a crash or garbage" U15:removed
run ram_bit7 "RAM bit 7 gone (U13, the schematic has it as U15): garbage, characters wrong" U13:removed
run no_ras "the DRAMs get no /RAS (U5 pin 63 open): no RAM, no K cursor" U5.63:open
run no_cas "the DRAMs get no /CAS (U5 pin 62 open): no RAM, no K cursor" U5.62:open
run no_we "the DRAMs get no /WE (U5 pin 64 open): RAM never written, no K cursor" U5.64:open
run no_video "no video (D19 open): the CPU runs and the picture is black" D19:open
run no_video_r1 "no video (R1 open): the CPU runs and the picture is black" R1:open
run unused_57 "U5 pin 57 is not connected to anything on the board: the same as healthy" U5.57:open
wait
for f in "$OUT"/*.txt; do
    echo "== $(basename "$f" .txt)"
    head -1 "$f"
    grep "^|" "$f" | grep -v "^|$" | tail -3
done
