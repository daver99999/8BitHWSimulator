/* DRAM decay check: a row of a 4116 that saw no /RAS for a long time loses its cells, a recently refreshed row keeps them. Build like the other tests: cc ... tests/atari800/decay.c */
#include "atari800.h"
#include "board.h"
#include "parts.h"
#include <stdio.h>
#include <string.h>
int main(void) {
    board_t *b = board_create(&machine_atari800);
    board_power_on(b);
    b->m->run(b, 40 * AT_CYCLES_PER_FRAME);
    sim_t *s = &b->sim;
    int pi = sim_find_part(s, "Z512_1");
    dram_state_t *st = s->parts[pi].state;
    memset(&st->cells[5 << 8], 1, 256);
    st->row_t[5] = (uint32_t)s->cycle - 100000;   /* row 5 was last refreshed 100000 cycles (56 ms) ago */
    b->m->run(b, 3000);
    int ones = 0; for (int c = 0; c < 256; c++) ones += st->cells[(5 << 8) | c];
    printf("row 5 cells still 1 after its next refresh: %d of 256 (expect 0: decayed)\n", ones);
    memset(&st->cells[6 << 8], 1, 256); st->row_t[6] = (uint32_t)s->cycle - 1000;
    b->m->run(b, 3000);
    ones = 0; for (int c = 0; c < 256; c++) ones += st->cells[(6 << 8) | c];
    printf("row 6 (last refreshed 0.5 ms ago): %d of 256 (expect 256: kept)\n", ones);
    return 0;
}
