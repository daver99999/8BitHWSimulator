/* Find the first opcode fetch (SYNC) of the 6502 whose address is in [LOW,HIGH] (hex): /tmp/atari_scan MAXCYCLES LOW HIGH [SKIP] */
#include "atari800.h"
#include "board.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    long max = atol(argv[1]);
    unsigned lo = (unsigned)strtol(argv[2], 0, 16), hi = (unsigned)strtol(argv[3], 0, 16);
    long skip = argc > 4 ? atol(argv[4]) : 0;
    board_t *b = board_create(&machine_atari800);
    board_power_on(b);
    sim_t *s = &b->sim;
    part_t *cpu = &s->parts[sim_find_part(s, "A303")];
    static const int ap[16] = { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23, 24, 25 };
    long steps = max * 4;
    for (long i = 0; i < steps; i++) {
        atari800_run_steps(b, 1);
        if (i < skip * 4) continue;
        if (!sim_read(s, part_pin(cpu, 7))) continue;
        unsigned a = 0;
        for (int j = 0; j < 16; j++) a |= (unsigned)sim_read(s, part_pin(cpu, ap[j])) << j;
        if (a >= lo && a <= hi) { printf("fetch at %04X, cycle %ld\n", a, i / 4); return 0; }
    }
    printf("none\n");
    return 0;
}
