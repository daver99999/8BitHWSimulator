/* Hold joystick directions (J102) while typing "print joy(0)" on the CPC 464: tests/cpc464/joy.c MASK out.ppm
   MASK: 1 up, 2 down, 4 left, 8 right, 16 fire 2, 32 fire 1 (the value JOY(0) should print) */
#include "cpc464.h"
#include "board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    int mask = argc > 1 ? atoi(argv[1]) : 1;
    board_t *b = board_create(&machine_cpc464);
    board_power_on(b);
    for (int f = 0; f < 120; f++) b->m->run(b, CPC_CYCLES_PER_FRAME);
    const char *text = "print joy(0)";
    int order[6] = { 0, 1, 2, 3, 5, 4 };                    /* JOY bit 0..5: up down left right fire2 fire1; the board's J102 index: fire 1 is 4, fire 2 is 5 */
    for (const char *c = text; ; c++) {
        int k = *c ? *c : 0x0D;
        if (!*c) for (int i = 0; i < 6; i++) if (mask & (1 << i)) b->m->key_down(b, 0xA0 + order[i]);   /* held for the RETURN only: a pushed direction shares the matrix with the keys, as on the board */
        b->m->key_down(b, k);
        for (int f = 0; f < 4; f++) { b->m->run(b, CPC_CYCLES_PER_FRAME); b->m->frame_update(b, 20000); }
        b->m->key_up(b, k);
        for (int f = 0; f < 3; f++) { b->m->run(b, CPC_CYCLES_PER_FRAME); b->m->frame_update(b, 20000); }
        if (!*c) break;
    }
    for (int f = 0; f < 20; f++) b->m->run(b, CPC_CYCLES_PER_FRAME);
    FILE *fp = fopen(argc > 2 ? argv[2] : "/tmp/joy.ppm", "wb");
    fprintf(fp, "P6\n%d %d\n255\n", CPC_SCREEN_W, CPC_SCREEN_H);
    for (int i = 0; i < CPC_SCREEN_W * CPC_SCREEN_H; i++) { uint32_t c = b->screen[i]; fputc(c & 255, fp); fputc((c >> 8) & 255, fp); fputc((c >> 16) & 255, fp); }
    fclose(fp);
    return 0;
}
