/* Headless check of the Atari 800: power on, run some frames, report the CPU, ANTIC and POKEY state and write the picture as a PPM.
 *   cc ... tests/atari800/boot.c $(SIM objects) -lm -o /tmp/atari_boot ; /tmp/atari_boot FRAMES [out.ppm] */
#include "atari800.h"
#include "board.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    int frames = argc > 1 ? atoi(argv[1]) : 10;
    board_t *b = board_create(&machine_atari800);
    board_power_on(b);
    atari800_t *z = AT(b);
    char buf[200];
    for (int f = 0; f < frames; f++) {
        b->m->run(b, AT_CYCLES_PER_FRAME);
        if (f % 10 == 9 || f == frames - 1 || frames <= 10) {
            pet_cpu_regs_of(&b->sim.parts[z->p_cpu], buf, sizeof(buf));
            char ab[200];
            atari_antic_status_of(&b->sim.parts[z->p_antic], ab, sizeof(ab));
            char pk[200];
            atari_pokey_keys_status(&b->sim.parts[z->p_pokey], pk, sizeof(pk));
            printf("frame %d: %s | %s | %s\n", f + 1, buf, ab, pk);
        }
    }
    if (argc > 2) {
        FILE *fp = fopen(argv[2], "wb");
        fprintf(fp, "P6\n%d %d\n255\n", AT_SCREEN_W, AT_SCREEN_H);
        for (int i = 0; i < AT_SCREEN_W * AT_SCREEN_H; i++) {
            uint32_t c = b->screen[i];
            fputc(c & 255, fp); fputc((c >> 8) & 255, fp); fputc((c >> 16) & 255, fp);
        }
        fclose(fp);
    }
    return 0;
}
