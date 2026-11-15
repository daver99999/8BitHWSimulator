/* pet_boot: run the PET board headless with a ROM in UD6 and print what happens.
 * pet_boot [-8] ud6.bin [cycles] */
#include "pet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    int a = 1;
    const machine_t *m = &machine_pet4032;
    if (a < argc && !strcmp(argv[a], "-8")) { m = &machine_pet8032; a++; }
    if (a >= argc) { fprintf(stderr, "usage: pet_boot [-8] ud6.bin [cycles]\n"); return 2; }
    int cycles = a + 1 < argc ? atoi(argv[a + 1]) : 20000;
    uint8_t rom[4096];
    FILE *f = fopen(argv[a], "rb");
    if (!f || fread(rom, 1, sizeof(rom), f) != sizeof(rom)) { fprintf(stderr, "bad rom\n"); return 2; }
    fclose(f);
    board_t *b = board_create(m);
    pet_set_rom(b, "UD6", rom, sizeof(rom));
    if (getenv("PET_CHAR")) {            /* PET_CHAR=file: a 2K character ROM */
        uint8_t ch[2048];
        FILE *cf = fopen(getenv("PET_CHAR"), "rb");
        if (cf && fread(ch, 1, sizeof(ch), cf) == sizeof(ch)) pet_set_rom(b, "UA3", ch, sizeof(ch));
        if (cf) fclose(cf);
    }
    if (getenv("PET_ROMS")) {            /* PET_ROMS=UD7=file,UD8=file...: more ROMs */
        char *l = strdup(getenv("PET_ROMS"));
        for (char *t = strtok(l, ","); t; t = strtok(NULL, ",")) {
            char *eq = strchr(t, '='); if (!eq) continue;
            *eq = 0;
            uint8_t buf[4096]; FILE *rf = fopen(eq + 1, "rb");
            size_t n = rf ? fread(buf, 1, sizeof(buf), rf) : 0;
            if (rf) fclose(rf);
            if (!pet_set_rom(b, t, buf, n)) fprintf(stderr, "can't use %s for %s\n", eq + 1, t);
        }
        free(l);
    }
    board_power_on(b);
    char regs[128];
    int step = cycles / 20 ? cycles / 20 : 1;
    for (int done = 0; done < cycles; done += step) {
        board_run(b, step);
        pet_cpu_regs_of(&b->sim.parts[PET(b)->p_cpu], regs, sizeof(regs));
        printf("t=%6d us  cpu cycles %6u  %s\n", done + step, pet_cpu_cycles(b), regs);
    }
    printf("display RAM:");
    for (int i = 0; i < 0x40; i++) printf("%s%02X", i % 16 ? " " : "\n  ", pet_vram_peek(b, i));
    printf("\n");
    if (getenv("PET_PPM")) {             /* PET_PPM=file: the picture */
        FILE *o = fopen(getenv("PET_PPM"), "wb");
        fprintf(o, "P6\n%d %d\n255\n", PET_SCREEN_W, PET_SCREEN_H);
        for (int i = 0; i < PET_SCREEN_W * PET_SCREEN_H; i++) {
            uint32_t c = b->screen[i];
            fputc(c & 255, o); fputc((c >> 8) & 255, o); fputc((c >> 16) & 255, o);
        }
        fclose(o);
    }
    return 0;
}
