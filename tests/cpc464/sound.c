/* The CPC 464's AY channel pins: type SOUND 1,284,100 (tone A, period 284 = 220 Hz at the AY's 1 MHz) and count the edges on net SND_A over some frames.
   tests/cpc464/sound.c: prints the edges on SND_A, SND_B, SND_C (only A should toggle) */
#include "cpc464.h"
#include "board.h"
#include <stdio.h>
#include <string.h>
static int nv(board_t *b, const char *name) { int n = sim_find_net(&b->sim, name); return n < 0 ? -1 : (int)(b->sim.nread[b->sim.nets[n].root] & 1); }
int main(void) {
    board_t *b = board_create(&machine_cpc464);
    board_power_on(b);
    for (int f = 0; f < 120; f++) b->m->run(b, CPC_CYCLES_PER_FRAME);
    const char *text = "sound 1,284,300";
    for (const char *c = text; ; c++) {
        int k = *c ? *c : 0x0D;
        b->m->key_down(b, k);
        for (int f = 0; f < 4; f++) { b->m->run(b, CPC_CYCLES_PER_FRAME); b->m->frame_update(b, 20000); }
        b->m->key_up(b, k);
        for (int f = 0; f < 3; f++) { b->m->run(b, CPC_CYCLES_PER_FRAME); b->m->frame_update(b, 20000); }
        if (!*c) break;
    }
    const char *n3[3] = { "SND_A", "SND_B", "SND_C" };
    int prev[3], edges[3] = { 0, 0, 0 };
    for (int i = 0; i < 3; i++) prev[i] = nv(b, n3[i]);
    int slice = CPC_CYCLES_PER_FRAME / 200;
    for (int f = 0; f < 10 * 200; f++) {
        b->m->run(b, slice);
        for (int i = 0; i < 3; i++) { int v = nv(b, n3[i]); if (v != prev[i]) edges[i]++; prev[i] = v; }
    }
    printf("edges in 10 frames: SND_A %d  SND_B %d  SND_C %d\n", edges[0], edges[1], edges[2]);
    return 0;
}
