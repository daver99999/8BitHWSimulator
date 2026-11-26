/* Who drives a net: /tmp/atari_who CYCLE0 NSTEPS REF:PIN  (every pin on the net with a drive other than Z, per step) */
#include "atari800.h"
#include "board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    long c0 = atol(argv[1]), n = atol(argv[2]);
    char ref[32]; char *colon = strchr(argv[3], ':');
    memcpy(ref, argv[3], (size_t)(colon - argv[3])); ref[colon - argv[3]] = 0;
    int pn = atoi(colon + 1);
    board_t *b = board_create(&machine_atari800);
    board_power_on(b);
    b->m->run(b, (int)c0);
    sim_t *s = &b->sim;
    int pin0 = part_pin(&s->parts[sim_find_part(s, ref)], pn);
    for (long i = 0; i < n; i++) {
        int root = s->pins[pin0].root;
        printf("%ld.%ld net=%s level=%d:", c0 + i / 4, i % 4, s->nets[s->pins[pin0].net].name, sim_read(s, pin0));
        for (int k = 0; k < s->npins; k++) {
            const pin_t *p = &s->pins[k];
            if (p->root != root || p->net < 0) continue;
            if ((p->eff >> 1) == S_Z) continue;
            printf(" %s.%d=%s%d", s->parts[p->part].ref, p->number, (p->eff >> 1) == S_PULL ? "p" : (p->eff >> 1) == S_RES ? "r" : "", p->eff & 1);
        }
        printf("\n");
        atari800_run_steps(b, 1);
    }
    return 0;
}
