#define CHIPS_IMPL
#include "ay8912.h"
#include <stdio.h>
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void wr(ay8912_t *a, int reg, int v) { ay8912_bus(a, AY_BUS_ADDR, (uint8_t)reg); ay8912_bus(a, AY_BUS_WRITE, (uint8_t)v); }
/* the edges of channel A's output over n clock ticks */
static int edges(ay8912_t *a, int n) {
    int e = 0; bool prev = a->out[0] > 0;
    for (int i = 0; i < n; i++) { ay8912_tick(a); bool v = a->out[0] > 0; if (v != prev) e++; prev = v; }
    return e;
}
int main(void) {
    ay8912_t a; ay8912_reset(&a);
    wr(&a, 0, 100); wr(&a, 1, 0); wr(&a, 7, 0x3E); wr(&a, 8, 15);       /* tone A, period 100, only channel A on, full volume */
    /* the pitch is f / (16 x period): 1.6e6 clocks at period 100 is 1000 cycles, 2000 edges */
    int e = edges(&a, 1600000);
    CHECK(e >= 1998 && e <= 2002);
    /* a period of 1 is the highest tone: f/16, so 16000 clocks are 1000 cycles */
    wr(&a, 0, 1);
    e = edges(&a, 16000);
    CHECK(e >= 1998 && e <= 2002);
    printf(fails ? "ay8912: %d FAILED\n" : "ay8912: ok\n", fails);
    return fails != 0;
}
