/* Bus trace of the Atari 800: from CYCLE0 print the address, data, R/W, HALT of N cycles, read at the end of PHI2 (what the 6502 sees).
 *   /tmp/atari_trace CYCLE0 N */
#include "atari800.h"
#include "board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int rd(board_t *b, const char *ref, int pin) {
    sim_t *s = &b->sim;
    int p = sim_find_part(s, ref);
    return sim_read(s, part_pin(&s->parts[p], pin));
}
int main(int argc, char **argv) {
    long c0 = argc > 1 ? atol(argv[1]) : 60000, n = argc > 2 ? atol(argv[2]) : 40;
    board_t *b = board_create(&machine_atari800);
    board_power_on(b);
    b->m->run(b, (int)c0);
    static const int ap[16] = { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23, 24, 25 };
    static const int dp[8] = { 33, 32, 31, 30, 29, 28, 27, 26 };
    for (long i = 0; i < n; i++) {
        for (int k = 0; k < 3; k++) b->m->run(b, 0), atari800_run_steps(b, 1);   /* steps 0..2: PHI2 high is seen up to here */
        unsigned a = 0, d = 0;
        for (int j = 0; j < 16; j++) a |= (unsigned)rd(b, "A303", ap[j]) << j;
        unsigned ba = 0;
        for (int j = 0; j < 16; j++) ba |= (unsigned)rd(b, "A303", ap[j]) << j;
        for (int j = 0; j < 8; j++) d |= (unsigned)rd(b, "A303", dp[j]) << j;
        printf("%6ld: cpu a=%04X d=%02X rw=%d halt=%d rdy=%d phi0=%d phi2=%d f0=%d nmi=%d res=%d\n", c0 + i, a, d, rd(b, "A303", 34), rd(b, "A302", 9),
               rd(b, "A303", 2), rd(b, "A303", 37), rd(b, "A301", 30), rd(b, "A301", 29), rd(b, "A303", 6), rd(b, "A303", 40));
        atari800_run_steps(b, 1);
    }
    return 0;
}
