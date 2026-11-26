/* Peek RAM through the DRAM cell arrays: /tmp/atari_ram FRAMES ADDR LEN */
#include "atari800.h"
#include "board.h"
#include "parts.h"
#include <stdio.h>
#include <stdlib.h>
static const int bit_chip[8] = {512, 511, 510, 508, 509, 507, 505, 506};
int main(int argc, char **argv) {
    int frames = atoi(argv[1]); unsigned addr = (unsigned)strtol(argv[2], 0, 0); int len = atoi(argv[3]);
    board_t *b = board_create(&machine_atari800);
    board_power_on(b);
    b->m->run(b, frames * 29868);
    sim_t *s = &b->sim;
    for (int i = 0; i < len; i++) {
        unsigned a = addr + (unsigned)i;
        int bank = a >> 14 & 3; if (bank > 2) { printf("--"); continue; }
        /* the 4116 model's address bit k is pin 5,7,6,12,11,10,13; the mux wires A6,A1,A2,A3,A4,A5,A0 (row) and A13,A8,A9,A10,A11,A12,A7 (col) there, inverted by the 74LS158 */
        static const int rl[7] = {6, 1, 2, 3, 4, 5, 0}, cl[7] = {13, 8, 9, 10, 11, 12, 7};
        unsigned row = 0, col = 0;
        for (int k = 0; k < 7; k++) { row |= (~(a >> rl[k]) & 1u) << k; col |= (~(a >> cl[k]) & 1u) << k; }
        int v = 0;
        for (int bt = 0; bt < 8; bt++) {
            char ref[16]; snprintf(ref, sizeof ref, "Z%d_%d", bit_chip[bt], bank + 1);
            int pi = sim_find_part(s, ref);
            dram_state_t *st = s->parts[pi].state;
            v |= (st->cells[(row << 8) | col] & 1) << bt;
        }
        if (i % 16 == 0) printf("%s%04X:", i ? "\n" : "", a);
        printf(" %02X", v);
    }
    printf("\n");
    return 0;
}
