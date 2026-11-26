/* Type a string on the Atari 800 keyboard and write the picture: /tmp/atari_keys "TEXT" out.ppm */
#include "atari800.h"
#include "board.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    board_t *b = board_create(&machine_atari800);
    board_power_on(b);
    if (getenv("CART")) { char m[200]; b->m->load(b, getenv("CART"), true, m, sizeof m); puts(m); }
    b->m->run(b, 120 * AT_CYCLES_PER_FRAME);
    for (const char *p = argv[1]; *p; p++) {
        board_key_down(b, *p == '\n' ? 13 : *p);
        for (int i = 0; i < 8; i++) { b->m->run(b, AT_CYCLES_PER_FRAME); b->m->frame_update(b, 16667); }
        board_key_up(b, *p == '\n' ? 13 : *p);
        for (int i = 0; i < 8; i++) { b->m->run(b, AT_CYCLES_PER_FRAME); b->m->frame_update(b, 16667); }
    }
    b->m->run(b, 5 * AT_CYCLES_PER_FRAME);
    b->m->print_text(b);
    FILE *fp = fopen(argv[2], "wb");
    fprintf(fp, "P6\n%d %d\n255\n", AT_SCREEN_W, AT_SCREEN_H);
    for (int i = 0; i < AT_SCREEN_W * AT_SCREEN_H; i++) { uint32_t c = b->screen[i]; fputc(c & 255, fp); fputc(c >> 8 & 255, fp); fputc(c >> 16 & 255, fp); }
    fclose(fp);
    return 0;
}
