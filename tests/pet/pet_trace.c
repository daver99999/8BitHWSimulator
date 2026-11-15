/* pet_trace: run the PET board, then print some nets once a microsecond.
 * pet_trace [-8] ud6.bin start_us count_us NET...   (busses: NAME[0-7] expands to NAME0..NAME7 as a hex number)
 * PET_CHAR=file loads a 2K character ROM. */
#include "pet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int level(board_t *b, const char *name) {
    int n = sim_find_net(&b->sim, name);
    if (n < 0) return -1;
    const net_t *r = &b->sim.nets[b->sim.nets[n].root];
    return r->strength == S_Z ? 2 : r->level;
}

int main(int argc, char **argv) {
    int a = 1;
    const machine_t *m = &machine_pet4032;
    if (a < argc && !strcmp(argv[a], "-8")) { m = &machine_pet8032; a++; }
    if (a + 3 >= argc) { fprintf(stderr, "usage: pet_trace [-8] ud6.bin start count NET...\n"); return 2; }
    uint8_t rom[4096];
    FILE *f = fopen(argv[a], "rb");
    if (!f || fread(rom, 1, sizeof(rom), f) != sizeof(rom)) { fprintf(stderr, "bad rom\n"); return 2; }
    fclose(f);
    int start = atoi(argv[a + 1]), count = atoi(argv[a + 2]);
    board_t *b = board_create(m);
    pet_set_rom(b, "UD6", rom, sizeof(rom));
    if (getenv("PET_CHAR")) {
        uint8_t ch[2048];
        FILE *cf = fopen(getenv("PET_CHAR"), "rb");
        if (cf && fread(ch, 1, sizeof(ch), cf) == sizeof(ch)) pet_set_rom(b, "UA3", ch, sizeof(ch));
        if (cf) fclose(cf);
    }
    board_power_on(b);
    board_run(b, start);
    int per = getenv("PET_STEPS") ? 1 : 0;      /* PET_STEPS=1: count and start in steps, not microseconds */
    if (per) pet_run_steps(b, 0);
    for (int i = 0; i < count; i++) {
        printf("%6d:", start + i);
        for (int k = a + 3; k < argc; k++) {
            size_t len = strlen(argv[k]);
            if (len > 2 && argv[k][len - 1] == ']') {       /* NAME[lo-hi] */
                char base[32]; int lo, hi;
                if (sscanf(argv[k], "%31[^[][%d-%d]", base, &lo, &hi) == 3) {
                    unsigned v = 0; char nm[48]; int bad = 0;
                    for (int j = lo; j <= hi; j++) {
                        snprintf(nm, sizeof(nm), "%s%d", base, j);
                        int l = level(b, nm);
                        if (l > 1) bad = 1;
                        v |= (unsigned)(l == 1) << (j - lo);
                    }
                    printf(" %s=%s%02X", base, bad ? "?" : "", v);
                    continue;
                }
            }
            int l = level(b, argv[k]);
            printf(" %s=%c", argv[k], l < 0 ? '!' : l == 2 ? 'z' : '0' + l);
        }
        printf("\n");
        if (per) pet_run_steps(b, 1); else board_run(b, 1);
    }
    return 0;
}
