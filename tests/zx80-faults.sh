#!/bin/sh
# tests/zx80-faults.sh - ZX80 (Issue 2) faults and what the monitor should show. The EXPECTED column is written from the circuit
# (the schematic and how the ZX80 ROM behaves), not from a real board: these faults are not all possible to make on the real
# machine, so none of them is "checked on a real board" unless a line says so. The simulator has no code that knows about
# any of these symptoms. Needs the ZX80 ROM in roms/zx80 (not shipped).
#
#   tests/zx80-faults.sh [outdir]
OUT=${1:-/tmp/zx80-faults}
BIN=${BIN:-./8BitHWSimulator}
FRAMES=${FRAMES:-600}
mkdir -p "$OUT"

run() {     # name "expected" faults...
    name=$1; expected=$2; shift 2
    args=""
    for f in "$@"; do args="$args --fault $f"; done
    {
        echo "EXPECTED: $expected"
        $BIN --machine zx80 --headless $FRAMES --text --shot "$OUT/$name.png" $args
    } > "$OUT/$name.txt" 2>&1 &
}

run healthy "a white screen with the K cursor at the bottom left"
run no_xtal "no 6.5 MHz clock: no picture, the CPU held in reset" X1:open
run no_ic20 "the oscillator gate is gone: no clock, no picture" IC20:removed
run no_cpu "no CPU: no K cursor, a blank picture" IC1:removed
run cpu_clk_open "the Z80 gets no clock (IC1 pin 6 open): no K cursor, a blank picture" IC1.6:open
run no_rom "no ROM: the CPU reads garbage, no K cursor" IC2:removed
run no_ic3 "RAM data bits 0, 1, 2 and 7 gone: a crash or garbage, no K cursor" IC3:removed
run no_ic4 "RAM data bits 3, 4, 5 and 6 gone: a crash or garbage, no K cursor" IC4:removed
run no_ic5 "the character latch is gone: no characters, a garbled picture" IC5:removed
run no_ic9 "the video shift register is gone: a black picture, the CPU runs" IC9:removed
run no_ic10 "the keyboard buffer is gone: the machine boots, the keys do nothing" IC10:removed
run no_ic6 "the address multiplexer for A8 and up is gone: a crash, no K cursor" IC6:removed
run no_ic7 "the address multiplexer for A3-A7 is gone: a crash, no K cursor" IC7:removed
run no_ic8 "the address multiplexer for A0-A2 is gone: a crash, no K cursor" IC8:removed
run no_nop "the NOP gates for D0-D5 are gone (IC14): the display file is not turned into NOPs: a broken or garbled picture" IC14:removed
run no_ic15 "IC15 drives the Z80 clock (and the NOP gates for D6 and D7): no clock, no picture, the CPU held in reset" IC15:removed
run no_ic21 "no line counter: the picture rolls or tears, the CPU runs" IC21:removed
run row_a11_open "the keyboard row on A11 is dead (D6 open): the keys on that row do nothing, the picture is normal" D6:open
run no_video "no video (R30 open): the CPU runs and the picture is black" R30:open
wait
for f in "$OUT"/*.txt; do
    echo "== $(basename "$f" .txt)"
    head -1 "$f"
    grep "^|" "$f" | grep -v "^|$" | tail -3
done
