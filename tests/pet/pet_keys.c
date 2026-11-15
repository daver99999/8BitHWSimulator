/* pet_keys: the keyboard test ROM (mkkbd.py), pressing keys and reading the scan back.
 * pet_keys [-8] ud6.bin */
#include "pet.h"
#include <stdio.h>
#include <string.h>

static void scan(board_t *b, const char *what) {
    board_run(b, 3000);
    printf("%-10s", what);
    for (int r = 0; r < 10; r++) printf(" %02X", pet_vram_peek(b, 0x40 + r));
    printf("\n");
}

int main(int argc, char **argv) {
    int a = 1;
    const machine_t *m = &machine_pet4032;
    if (a < argc && !strcmp(argv[a], "-8")) { m = &machine_pet8032; a++; }
    uint8_t rom[4096];
    FILE *f = a < argc ? fopen(argv[a], "rb") : NULL;
    if (!f || fread(rom, 1, sizeof(rom), f) != sizeof(rom)) { fprintf(stderr, "bad rom\n"); return 2; }
    fclose(f);
    board_t *b = board_create(m);
    pet_set_rom(b, "UD6", rom, sizeof(rom));
    board_power_on(b);
    board_run(b, 20000);
    scan(b, "none");
    const char keys[] = "QA1\r";
    for (const char *k = keys; *k; k++) {
        int key = (*k == '\r') ? 0x0D : *k;
        board_key_down(b, key);
        scan(b, (char[]){ '\'', *k == '\r' ? 'R' : *k, '\'', 0 });
        board_key_up(b, key);
        for (int i = 0; i < 4; i++) { board_frame_update_hook: board_run(b, 17000); if (b->m->frame_update) b->m->frame_update(b, 17000); }
        scan(b, "released");
    }
    return 0;
}
