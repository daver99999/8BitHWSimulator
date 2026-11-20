#!/bin/sh
# tests/ts1000.sh - the TS1000/ZX81 boots, takes the keyboard and keeps its display going.
#
#   tests/ts1000.sh [binary]
#
# Types PRINT 2+3 (P, 2, +, 3 and NEWLINE: the ZX81's one-key keywords) a few frames
# apart, so the ROM sees a second key after the first. Once, a second key froze the CPU
# in /WAIT for good (TR1, the /WAIT transistor, did not see /NMI go high), so the picture
# stopped: the text read from the display file must show the answer, 5, on its first line with text.
BIN=${1:-./8BitHWSimulator}
export C64HW_KEYFRAMES=16       # the ZX81 ROM debounces: a key less than about 15 frames after the last is dropped
fail=0
check() {   # name expected-first-line machine-options... (the typed text is in TYPE)
    name=$1; want=$2; shift 2
    got=$(C64HW_TYPE="$TYPE" "$BIN" --headless 1200 --text "$@" 2>&1 | awk '/^CPU:/ {go=1; next} go && $0 != "" {print; exit}')
    if [ "$got" = "$want" ]; then echo "ok   $name"; else echo "FAIL $name: first text line '$got', wanted '$want'"; fail=1; fi
}
TYPE='P2+3
' check "PRINT 2+3, 2K"           "5" --machine ts1000
TYPE='P2+3
' check "PRINT 2+3, 1K (ZX81)"    "5" --machine ts1000 --option ram=1k
TYPE='P"HI"
' check "PRINT \"HI\""            "HI" --machine ts1000
exit $fail
