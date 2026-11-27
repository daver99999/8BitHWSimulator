/* Trace the CPC 464's bus at the 16 MHz tick: tests/cpc464/trace.c FRAMES [TICKS]  */
#include "cpc464.h"
#include "board.h"
#include <stdio.h>
#include <stdlib.h>
static int nv(board_t *b, const char *name) { int n = sim_find_net(&b->sim, name); return n < 0 ? -1 : (int)(b->sim.nread[b->sim.nets[n].root] & 1); }
static int bus(board_t *b, const char *fmt, int n) { int v = 0; char nm[16]; for (int i = 0; i < n; i++) { snprintf(nm, sizeof nm, fmt, i); v |= nv(b, nm) << i; } return v; }
int main(int argc, char **argv) {
    int frames = argc > 1 ? atoi(argv[1]) : 5, ticks = argc > 2 ? atoi(argv[2]) : 64;
    board_t *b = board_create(&machine_cpc464);
    board_power_on(b);
    for (int f = 0; f < frames; f++) b->m->run(b, CPC_CYCLES_PER_FRAME);
    printf("tick CLK RDY MREQ IORQ RD WR M1 HALT INT |A  D |RAS CAS CA CPUA MWE ROMEN RAMRD 244 RD-bus RESET NMI BUSRQ\n");
    for (int i = 0; i < ticks; i++) {
        b->m->run(b, 1);
        printf("%3d  %d   %d   %d    %d    %d  %d  %d  %d    %d  |%04X %02X |%d   %d   %d  %d    %d   %d     %d     %d   %02X     %d   %d   %d\n", i,
               nv(b, "CLKPHI"), nv(b, "READY"), nv(b, "MREQ"), nv(b, "IORQ"), nv(b, "RD"), nv(b, "WR"), nv(b, "M1"), nv(b, "HALT"), nv(b, "INT"),
               bus(b, "A%d", 16), bus(b, "D%d", 8), nv(b, "RAS"), nv(b, "CAS"), nv(b, "CASADDR"), nv(b, "CPUADDR"), nv(b, "MWE"), nv(b, "ROMEN"), nv(b, "RAMRD"), nv(b, "N244EN"),
               bus(b, "RD%d", 8), nv(b, "RESET"), nv(b, "NMI"), nv(b, "BUSRQ"));
    }
    return 0;
}
