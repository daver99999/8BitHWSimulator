/* Print pins step by step: /tmp/atari_pins CYCLE0 NSTEPS REF:PINS [REF:PINS...]   (PINS: 1,2,5-8)  levels 0/1, z = floating */
#include "atari800.h"
#include "board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    long c0 = atol(argv[1]), n = atol(argv[2]);
    board_t *b = board_create(&machine_atari800);
    board_power_on(b);
    b->m->run(b, (int)c0);
    sim_t *s = &b->sim;
    for (long i = 0; i < n; i++) {
        printf("%ld.%ld ", c0 + i / 4, i % 4);
        for (int a = 3; a < argc; a++) {
            char ref[32]; char *colon = strchr(argv[a], ':');
            size_t rl = (size_t)(colon - argv[a]); memcpy(ref, argv[a], rl); ref[rl] = 0;
            int pi = sim_find_part(s, ref);
            printf("%s[", ref);
            char *list = strdup(colon + 1);
            for (char *tok = strtok(list, ","); tok; tok = strtok(NULL, ",")) {
                int lo = atoi(tok), hi = lo; char *dash = strchr(tok, '-'); if (dash) hi = atoi(dash + 1);
                for (int p = lo; p <= hi; p++) {
                    int pin = part_pin(&s->parts[pi], p);
                    int net = s->pins[pin].net;
                    int st = net >= 0 ? s->nets[s->pins[pin].root >= 0 ? s->pins[pin].root : net].strength : 0;
                    printf("%d", sim_read(s, pin));
                    (void)st;
                }
                printf(" ");
            }
            printf("] ");
            free(list);
        }
        printf("\n");
        atari800_run_steps(b, 1);
    }
    return 0;
}
