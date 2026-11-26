/* Bus address/data at the end of each cycle: /tmp/atari_dma CYCLE0 N  (halt and refresh marked) */
#include "atari800.h"
#include "board.h"
#include <stdio.h>
#include <stdlib.h>
static int bus(sim_t *s, const char *pfx, int n) {
    int v = 0;
    for (int i = 0; i < n; i++) { char nm[8]; snprintf(nm, sizeof nm, "%s%d", pfx, i); int ni = sim_find_net(s, nm); if (ni >= 0 && s->nets[ni].level) v |= 1 << i; }
    return v;
}
int main(int argc, char **argv) {
    long c0 = atol(argv[1]), n = atol(argv[2]);
    board_t *b = board_create(&machine_atari800);
    board_power_on(b);
    b->m->run(b, (int)c0);
    sim_t *s = &b->sim;
    int nh = sim_find_net(s, "HALT"), nr = sim_find_net(s, "REF"), nw = sim_find_net(s, "R/W");
    for (long i = 0; i < n; i++) {
        int halt = 0, ref = 0, a = 0, d = 0, rw = 0;
        for (int st = 0; st < 4; st++) {
            atari800_run_steps(b, 1);
            if (st == 1) { a = bus(s, "A", 16); halt = s->nets[nh].level; ref = s->nets[nr].level; rw = s->nets[nw].level; }
            if (st == 3) d = bus(s, "D", 8);
        }
        printf("%ld: a=%04X d=%02X rw=%d halt=%d ref=%d\n", c0 + i, a, d, rw, halt, ref);
    }
    return 0;
}
