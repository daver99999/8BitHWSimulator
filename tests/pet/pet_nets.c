/* pet_nets: run the PET board and print the edge count and duty of some nets.
 * pet_nets [-8] ud6.bin cycles NET... */
#include "pet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    int a = 1;
    const machine_t *m = &machine_pet4032;
    if (a < argc && !strcmp(argv[a], "-8")) { m = &machine_pet8032; a++; }
    if (a + 2 >= argc) { fprintf(stderr, "usage: pet_nets [-8] ud6.bin cycles NET...\n"); return 2; }
    uint8_t rom[4096];
    FILE *f = fopen(argv[a], "rb");
    if (!f || fread(rom, 1, sizeof(rom), f) != sizeof(rom)) { fprintf(stderr, "bad rom\n"); return 2; }
    fclose(f);
    int cycles = atoi(argv[a + 1]);
    board_t *b = board_create(m);
    pet_set_rom(b, "UD6", rom, sizeof(rom));
    board_power_on(b);
    board_run(b, 6000);                     /* past the reset */
    uint32_t *rises = calloc((size_t)b->sim.nnets, 4), *highs = calloc((size_t)b->sim.nnets, 4);
    board_edges_start(b);
    board_run(b, cycles);
    uint32_t steps = board_edges_stop(b, rises, highs);
    printf("%u steps (%d us)\n", steps, cycles);
    for (int i = a + 2; i < argc; i++) {
        int n = sim_find_net(&b->sim, argv[i]);
        if (n < 0) { printf("%-12s ?\n", argv[i]); continue; }
        printf("%-12s rises %6u  high %5.1f%%\n", argv[i], rises[n], 100.0 * highs[n] / steps);
    }
    return 0;
}
