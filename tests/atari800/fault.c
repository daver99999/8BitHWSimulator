/* Open one pin and see what the machine does: /tmp/atari_fault REF PIN [frames] -> prints the screen text and the CPU state */
#include "atari800.h"
#include "board.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    board_t *b = board_create(&machine_atari800);
    board_power_on(b);
    sim_t *s = &b->sim;
    int pi = sim_find_part(s, argv[1]);
    if (pi < 0) { printf("no part %s\n", argv[1]); return 1; }
    board_set_pin_fault(b, part_pin(&s->parts[pi], atoi(argv[2])), PF_OPEN);
    board_rebuild(b);
    int frames = argc > 3 ? atoi(argv[3]) : 150;
    b->m->run(b, frames * AT_CYCLES_PER_FRAME);
    char buf[200];
    b->m->cpu_regs(b, buf, sizeof buf);
    printf("%s\n", buf);
    b->m->print_text(b);
    return 0;
}
