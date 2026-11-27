/* The CPC 464's tape port: type SAVE "T", RETURN, then a key at "Press REC and PLAY then any key:". Prints the MOTOR line (CP001 pin 8, pulled low by Q101
   when the 8255's PC4 is high) and the edges on WRDATA (CP001 pin 4, the 8255's PC5) before and after the key. tests/cpc464/tape.c [FAULT_SPEC...] */
#include "cpc464.h"
#include "board.h"
#include <stdio.h>
static int nv(board_t *b, const char *name) { int n = sim_find_net(&b->sim, name); return n < 0 ? -1 : (int)(b->sim.nread[b->sim.nets[n].root] & 1); }
static int prev_w, edges;
static void run_frames(board_t *b, int frames) {
    for (int f = 0; f < frames; f++) {
        for (int i = 0; i < CPC_CYCLES_PER_FRAME / 16; i++) { b->m->run(b, 16); int w = nv(b, "WRDATA"); if (w != prev_w) edges++; prev_w = w; }
        b->m->frame_update(b, 20000);
    }
}
static void press(board_t *b, int k) { b->m->key_down(b, k); run_frames(b, 4); b->m->key_up(b, k); run_frames(b, 3); }
int main(int argc, char **argv) {
    board_t *b = board_create(&machine_cpc464);
    for (int i = 1; i < argc; i++) if (!board_apply_fault_spec(b, argv[i])) { printf("bad spec %s\n", argv[i]); return 1; }
    board_power_on(b);
    run_frames(b, 120);
    printf("at the prompt: MOTORB (PC4) %d, MOTOR %d\n", nv(b, "MOTORB"), nv(b, "MOTOR"));
    for (const char *c = "save \"t\""; *c; c++) press(b, *c);
    press(b, 0x0D);
    run_frames(b, 20);
    edges = 0; run_frames(b, 10);
    printf("at 'Press REC and PLAY': MOTORB %d, MOTOR %d, WRDATA edges in 10 frames %d\n", nv(b, "MOTORB"), nv(b, "MOTOR"), edges);
    press(b, ' ');
    for (int chunk = 0; chunk < 8; chunk++) {      /* the ROM waits for the motor to run up before it writes the header */
        edges = 0; run_frames(b, 50);
        printf("saving, frames %3d-%3d: MOTORB %d, MOTOR %d, WRDATA edges %d\n", chunk * 50, chunk * 50 + 49, nv(b, "MOTORB"), nv(b, "MOTOR"), edges);
    }
    FILE *fp = fopen("/tmp/tape.ppm", "wb"); fprintf(fp, "P6\n%d %d\n255\n", CPC_SCREEN_W, CPC_SCREEN_H);
    for (int i = 0; i < CPC_SCREEN_W * CPC_SCREEN_H; i++) { uint32_t c = b->screen[i]; fputc(c & 255, fp); fputc((c >> 8) & 255, fp); fputc((c >> 16) & 255, fp); }
    fclose(fp);
    return 0;
}
