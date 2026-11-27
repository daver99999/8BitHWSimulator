/* Boot a machine with faults and report what it does.
 *   faultrun MACHINE FRAMES OUT.ppm SPEC...   (SPEC as board_apply_fault_spec: "IC117:removed", "IC103.11:open") */
#include "board.h"
#include "cpc464.h"
#include "atari800.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    if (argc < 4) return 2;
    const machine_t *m = !strcmp(argv[1], "cpc464") ? &machine_cpc464 : &machine_atari800;
    int frames = atoi(argv[2]);
    board_t *b = board_create(m);
    board_power_on(b);
    for (int i = 4; i < argc; i++)
        if (!board_apply_fault_spec(b, argv[i])) { printf("bad spec %s\n", argv[i]); return 1; }
    board_rebuild(b);
    int per = m->cycles_per_frame;
    for (int f = 0; f < frames; f++) { b->m->run(b, per); b->m->frame_update(b, 20000); }
    char buf[200] = "";
    if (b->m->cpu_regs) b->m->cpu_regs(b, buf, sizeof buf);
    if (m == &machine_atari800 && b->m->print_text) b->m->print_text(b);
    uint32_t bg = b->screen[0]; long diff = 0; int w = m->screen_w, h = m->screen_h;
    for (int i = 0; i < w * h; i++) if (b->screen[i] != bg) diff++;
    printf("%s | %s | non-bg pixels %ld\n", argv[4], buf, diff);
    FILE *fp = fopen(argv[3], "wb");
    fprintf(fp, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) { uint32_t c = b->screen[i]; fputc(c & 255, fp); fputc((c >> 8) & 255, fp); fputc((c >> 16) & 255, fp); }
    fclose(fp);
    return 0;
}
