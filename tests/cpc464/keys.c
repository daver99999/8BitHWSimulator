/* Type a line on the CPC 464's keyboard after the boot and save the picture: tests/cpc464/keys.c "print 6*7" out.ppm */
#include "cpc464.h"
#include "board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    board_t *b = board_create(&machine_cpc464);
    board_power_on(b);
    for (int f = 0; f < 120; f++) b->m->run(b, CPC_CYCLES_PER_FRAME);
    const char *text = argc > 1 ? argv[1] : "print 6*7";
    for (const char *c = text; ; c++) {
        int k = *c ? *c : 0x0D;
        b->m->key_down(b, k);
        for (int f = 0; f < 4; f++) { b->m->run(b, CPC_CYCLES_PER_FRAME); b->m->frame_update(b, 20000); }
        b->m->key_up(b, k);
        for (int f = 0; f < 3; f++) { b->m->run(b, CPC_CYCLES_PER_FRAME); b->m->frame_update(b, 20000); }
        if (!*c) break;
    }
    for (int f = 0; f < 20; f++) b->m->run(b, CPC_CYCLES_PER_FRAME);
    FILE *fp = fopen(argc > 2 ? argv[2] : "/tmp/keys.ppm", "wb");
    fprintf(fp, "P6\n%d %d\n255\n", CPC_SCREEN_W, CPC_SCREEN_H);
    for (int i = 0; i < CPC_SCREEN_W * CPC_SCREEN_H; i++) { uint32_t c = b->screen[i]; fputc(c & 255, fp); fputc((c >> 8) & 255, fp); fputc((c >> 16) & 255, fp); }
    fclose(fp);
    return 0;
}
