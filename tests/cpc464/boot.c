/* Headless check of the CPC 464: power on, run some frames, report the Z80 and write the picture as a PPM.
 *   cc ... tests/cpc464/boot.c $(SIM objects except main/ui) -lm -o /tmp/cpc_boot ; /tmp/cpc_boot FRAMES [out.ppm] */
#include "cpc464.h"
#include "board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    int frames = argc > 1 ? atoi(argv[1]) : 10;
    board_t *b = board_create(&machine_cpc464);
    board_power_on(b);
    cpc464_t *z = CPC(b);
    char buf[200];
    printf("status: %s\n", b->m->cpu_status(b));
    for (int f = 0; f < frames; f++) {
        b->m->run(b, CPC_CYCLES_PER_FRAME);
        if (f % 5 == 4 || f == frames - 1 || frames <= 10) {
            b->m->cpu_regs(b, buf, sizeof(buf));
            char ga[300]; cpc464_ga_status(b, ga, sizeof ga);
            printf("frame %d: %s | %s\n", f + 1, buf, ga);
        }
    }
    if (argc > 2) {
        FILE *fp = fopen(argv[2], "wb");
        fprintf(fp, "P6\n%d %d\n255\n", CPC_SCREEN_W, CPC_SCREEN_H);
        for (int i = 0; i < CPC_SCREEN_W * CPC_SCREEN_H; i++) {
            uint32_t c = b->screen[i];
            fputc(c & 255, fp); fputc((c >> 8) & 255, fp); fputc((c >> 16) & 255, fp);
        }
        fclose(fp);
    }
    (void)z;
    return 0;
}
