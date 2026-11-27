/* The CPC 464's printer port: BUSY (PRN pin 11) is held low (a printer that is ready), then PRINT #8,"A" is typed and the byte on PRN pins 2-8
   is read at every falling edge of /STROBE (PRN pin 1). tests/cpc464/printer.c [FAULT_SPEC...]; expected: strobe 1 data 0x41 ('A'), then 0x0D, 0x0A */
#include "cpc464.h"
#include "board.h"
#include <stdio.h>
static int nv(board_t *b, const char *name) { int n = sim_find_net(&b->sim, name); return n < 0 ? -1 : (int)(b->sim.nread[b->sim.nets[n].root] & 1); }
int main(int argc, char **argv) {
    board_t *b = board_create(&machine_cpc464);
    if (!board_apply_fault_spec(b, "IC107.24:gnd")) { printf("BUSY fault not applied\n"); return 1; }          /* the 8255's BUSY input held low */
    for (int i = 1; i < argc; i++) if (!board_apply_fault_spec(b, argv[i])) { printf("bad spec %s\n", argv[i]); return 1; }
    board_power_on(b);
    for (int f = 0; f < 120; f++) b->m->run(b, CPC_CYCLES_PER_FRAME);
    const char *text = "print #8,\"A\"";
    static const char *q[7] = { "PRQ0", "PRQ1", "PRQ2", "PRQ3", "PRQ4", "PRQ5", "PRQ6" };
    int prev = nv(b, "PRSTRB"), got = 0;
    for (const char *c = text; ; c++) {
        int k = *c ? *c : 0x0D;
        b->m->key_down(b, k);
        for (int f = 0; f < 4; f++) {
            for (int i = 0; i < CPC_CYCLES_PER_FRAME / 16; i++) {
                b->m->run(b, 16);
                int v = nv(b, "PRSTRB");
                if (prev && !v) { int d = 0; for (int j = 0; j < 7; j++) d |= nv(b, q[j]) << j; printf("strobe %d: data %02X\n", ++got, d); }
                prev = v;
            }
            b->m->frame_update(b, 20000);
        }
        b->m->key_up(b, k);
        for (int f = 0; f < 3; f++) { for (int i = 0; i < CPC_CYCLES_PER_FRAME / 16; i++) { b->m->run(b, 16); int v = nv(b, "PRSTRB");
            if (prev && !v) { int d = 0; for (int j = 0; j < 7; j++) d |= nv(b, q[j]) << j; printf("strobe %d: data %02X\n", ++got, d); } prev = v; } b->m->frame_update(b, 20000); }
        if (!*c) break;
    }
    for (int f = 0; f < 30; f++) for (int i = 0; i < CPC_CYCLES_PER_FRAME / 16; i++) { b->m->run(b, 16); int v = nv(b, "PRSTRB");
        if (prev && !v) { int d = 0; for (int j = 0; j < 7; j++) d |= nv(b, q[j]) << j; printf("strobe %d: data %02X\n", ++got, d); } prev = v; }
    if (!got) printf("no strobe; BUSY %d PRCK %d PRSTRB %d PRQ7 %d\n", nv(b, "BUSY"), nv(b, "PRCK"), nv(b, "PRSTRB"), nv(b, "PRQ7"));
    { FILE *fp = fopen("/tmp/prn.ppm", "wb"); fprintf(fp, "P6\n%d %d\n255\n", CPC_SCREEN_W, CPC_SCREEN_H); for (int i = 0; i < CPC_SCREEN_W * CPC_SCREEN_H; i++) { uint32_t c = b->screen[i]; fputc(c & 255, fp); fputc((c >> 8) & 255, fp); fputc((c >> 16) & 255, fp); } fclose(fp); }
    return 0;
}
