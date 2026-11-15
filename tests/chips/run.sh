#!/bin/sh
# Unit tests of the PET chip cores. Run from anywhere: sh tests/chips/run.sh
cd "$(dirname "$0")" || exit 1
rc=0
for t in m6520 m6522 m6545; do
    cc -Wall -Wextra -I../../chips -o /tmp/test_$t test_$t.c || { rc=1; continue; }
    /tmp/test_$t || rc=1
done
cc -Wall -Wextra -Wno-unused-parameter -I../../src -I../../chips -o /tmp/test_pet_parts test_pet_parts.c ../../src/sim.c ../../src/parts.c ../../src/pet_parts.c -lm || rc=1
/tmp/test_pet_parts || rc=1
exit $rc
