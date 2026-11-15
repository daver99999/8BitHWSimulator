#define CHIPS_IMPL
#include "m6520.h"
#include <stdio.h>
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static uint64_t wr(m6520_t* c, uint64_t base, int rs, int v) {
    uint64_t p = (base & ~(M6520_RS|M6520_RW)) | M6520_CS | (uint64_t)rs; M6520_SET_DATA(p, v);
    return m6520_tick(c, p);
}
static uint64_t rd(m6520_t* c, uint64_t base, int rs, int* v) {
    uint64_t p = (base & ~M6520_RS) | M6520_CS | M6520_RW | (uint64_t)rs;
    p = m6520_tick(c, p); *v = M6520_GET_DATA(p); return p;
}
int main(void) {
    m6520_t c; m6520_init(&c);
    uint64_t in = 0; int v;
    // DDR vs data register select
    wr(&c, in, 1, 0x00);        // CRA: DDR visible
    wr(&c, in, 0, 0xF0);        // DDRA = F0
    wr(&c, in, 1, 0x04);        // CRA: data visible
    wr(&c, in, 0, 0xA5);        // ORA
    uint64_t pins = 0; M6520_SET_PAB(pins, 0x0C, 0);
    pins = rd(&c, pins, 0, &v);
    CHECK(v == 0xAC);           // output bits from ORA (A0), input bits from pins (0C)
    CHECK(M6520_GET_PA(pins) == 0xAC);
    // CA1 falling edge sets flag 7 and IRQ with bit0 enable
    wr(&c, in, 1, 0x05);                       // CRA: data, IRQ1 enable, falling edge
    pins = m6520_tick(&c, M6520_CA1);          // CA1 high (rising: not active)
    rd(&c, M6520_CA1, 1, &v); CHECK((v & 0x80) == 0);
    pins = m6520_tick(&c, 0);                  // falling: active
    CHECK(pins & M6520_IRQA);
    rd(&c, 0, 1, &v); CHECK(v & 0x80);
    rd(&c, 0, 0, &v);                          // read port A clears the flag
    pins = m6520_tick(&c, 0);
    CHECK(!(pins & M6520_IRQA));
    // rising edge option
    wr(&c, 0, 1, 0x07);
    m6520_tick(&c, 0); pins = m6520_tick(&c, M6520_CA1); CHECK(pins & M6520_IRQA);
    rd(&c, M6520_CA1, 0, &v);
    // CA2 as input, bit 6 flag, enable via bit 3
    wr(&c, M6520_CA1, 1, 0x1F);                // IRQ1 en, rising CA1, CA2 input rising, IRQ2 en
    pins = m6520_tick(&c, M6520_CA1 | M6520_CA2);
    rd(&c, M6520_CA1 | M6520_CA2, 1, &v); CHECK(v & 0x40);
    // CA2 output handshake: low after read of port A, high on active CA1 edge
    m6520_init(&c);
    wr(&c, 0, 1, 0x26);                        // CA2 handshake out (CR5=1,CR4=0,CR3=0), data reg, CA1 rising
    pins = m6520_tick(&c, 0); CHECK(pins & M6520_CA2);          // idle high
    pins = rd(&c, 0, 0, &v);
    pins = m6520_tick(&c, 0); CHECK(!(pins & M6520_CA2));       // low after the read
    pins = m6520_tick(&c, M6520_CA1); CHECK(pins & M6520_CA2);  // CA1 rising ends it
    // CA2 pulse mode: low for one cycle
    wr(&c, 0, 1, 0x2C);
    pins = rd(&c, 0, 0, &v); CHECK(!(pins & M6520_CA2));
    pins = m6520_tick(&c, 0); CHECK(pins & M6520_CA2);
    // CA2 manual output
    wr(&c, 0, 1, 0x3C); pins = m6520_tick(&c, 0); CHECK(pins & M6520_CA2);
    wr(&c, 0, 1, 0x34); pins = m6520_tick(&c, 0); CHECK(!(pins & M6520_CA2));
    // port B: reads ORB for output lines; CB2 write handshake
    m6520_init(&c);
    wr(&c, 0, 3, 0x00); wr(&c, 0, 2, 0xFF); wr(&c, 0, 3, 0x26);  // DDRB = FF, CB2 handshake, CB1 rising
    wr(&c, 0, 2, 0x5A);
    pins = m6520_tick(&c, 0); CHECK(!(pins & M6520_CB2));
    CHECK(M6520_GET_PB(pins) == 0x5A);
    pins = m6520_tick(&c, M6520_CB1); CHECK(pins & M6520_CB2);
    // reset
    pins = m6520_tick(&c, M6520_RES); CHECK(c.cra == 0 && c.ddra == 0 && !(pins & (M6520_IRQA|M6520_IRQB)));
    printf(fails ? "m6520: %d FAILED\n" : "m6520: all ok\n", fails);
    return fails != 0;
}
