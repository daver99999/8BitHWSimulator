#!/bin/sh
# tests/ts1500.sh - the TS1500 boots, takes the keyboard and keeps its display going.
#
#   tests/ts1500.sh [binary]
#
# Types PRINT 2+3 (P, 2, +, 3 and NEWLINE) a few frames apart, as tests/ts1000.sh does, and reads
# the answer, 5, from the first line of text on the display. The Z80's clock is on U5 pin 58 and the
# keyboard rows go through D10-D17 in the order measured on a real board, so a wrong netlist stops this.
# The TS1500's own ROM is not shipped: put it in roms/ts1500 (the test is skipped without it).
BIN=${1:-./8BitHWSimulator}
if ! ls roms/ts1500/* >/dev/null 2>&1; then echo "skip ts1500: no ROM in roms/ts1500"; exit 0; fi
export C64HW_KEYFRAMES=16
fail=0
check() {   # name expected-first-line machine-options... (the typed text is in TYPE)
    name=$1; want=$2; shift 2
    got=$(C64HW_TYPE="$TYPE" "$BIN" --headless 1000 --text "$@" 2>&1 | awk '/^CPU:/ {go=1; next} go && $0 != "" {print; exit}')
    if [ "$got" = "$want" ]; then echo "ok   $name"; else echo "FAIL $name: first text line '$got', wanted '$want'"; fail=1; fi
}
TYPE='P2+3
' check "PRINT 2+3, TS1500" "5" --machine ts1500
exit $fail
