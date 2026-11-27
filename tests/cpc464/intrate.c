/* The CPC 464's /INT: count its falling edges over 25 frames after the boot (expected about 6 a frame, 300 Hz) */
#include "cpc464.h"
#include "board.h"
#include <stdio.h>
static int nv(board_t *b, const char *name) { int n = sim_find_net(&b->sim, name); return n < 0 ? -1 : (int)(b->sim.nread[b->sim.nets[n].root] & 1); }
int main(void) {
    board_t *b = board_create(&machine_cpc464);
    board_power_on(b);
    for (int f = 0; f < 120; f++) b->m->run(b, CPC_CYCLES_PER_FRAME);
    int prev = nv(b, "INT"), fall = 0, slice = 16;
    for (int i = 0; i < 25 * (CPC_CYCLES_PER_FRAME / 16); i++) { b->m->run(b, slice); int v = nv(b, "INT"); if (prev && !v) fall++; prev = v; }
    printf("/INT falling edges in 25 frames: %d (%.2f a frame)\n", fall, fall / 25.0);
    return 0;
}
