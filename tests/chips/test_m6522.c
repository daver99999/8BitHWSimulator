#define CHIPS_IMPL
#include "m6522.h"
#include <stdio.h>
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static uint64_t in_pins;  // external levels (CA1 etc.)
static uint64_t idle(m6522_t* c) { return m6522_tick(c, in_pins); }
static uint64_t wr(m6522_t* c, int rs, int v) {
    uint64_t p = (in_pins & ~(M6522_RS|M6522_RW)) | M6522_CS | (uint64_t)rs; M6522_SET_DATA(p, v);
    return m6522_tick(c, p);
}
static int rd(m6522_t* c, int rs) {
    uint64_t p = (in_pins & ~M6522_RS) | M6522_CS | M6522_RW | (uint64_t)rs;
    p = m6522_tick(c, p); return M6522_GET_DATA(p);
}
int main(void) {
    m6522_t c; m6522_init(&c); in_pins = 0; uint64_t p;
    // ---- T1 one-shot, N=10: IRQ N+2 cycles after the write cycle
    m6522_reset(&c);
    wr(&c, M6522_REG_IER, 0x80 | M6522_IRQ_T1);
    wr(&c, M6522_REG_T1CL, 10);
    wr(&c, M6522_REG_T1CH, 0);
    int n = 0; p = 0;
    while (!(p & M6522_IRQ) && n < 100) { p = idle(&c); n++; }
    CHECK(n == 12);
    CHECK(rd(&c, M6522_REG_IFR) == (0x80 | M6522_IRQ_T1));
    rd(&c, M6522_REG_T1CL);                         // clears the flag
    CHECK(rd(&c, M6522_REG_IFR) == 0);
    // one-shot does not fire again
    for (int i = 0; i < 100; i++) p = idle(&c);
    CHECK(!(p & M6522_IRQ));
    // ---- T1 free run N=10, PB7 toggling: period N+2 = 12
    m6522_reset(&c);
    wr(&c, M6522_REG_ACR, 0xC0);                    // free-run + PB7
    wr(&c, M6522_REG_T1CL, 10);
    p = wr(&c, M6522_REG_T1CH, 0);
    CHECK(!(M6522_GET_PB(p) & 0x80));               // PB7 low after the start
    int toggles = 0, last = 0, first = -1, second = -1;
    for (int i = 1; i <= 100; i++) {
        p = idle(&c); int pb7 = (M6522_GET_PB(p) >> 7) & 1;
        if (pb7 != last) { toggles++; if (first < 0) first = i; else if (second < 0) second = i; }
        last = pb7;
    }
    CHECK(toggles >= 7);
    CHECK(second - first == 12);
    // ---- T2 one-shot
    m6522_reset(&c);
    wr(&c, M6522_REG_T2CL, 20);
    wr(&c, M6522_REG_T2CH, 0);
    n = 0; while (!(c.ifr & M6522_IRQ_T2) && n < 100) { idle(&c); n++; }
    CHECK(n == 22);
    CHECK(rd(&c, M6522_REG_IFR) & M6522_IRQ_T2);
    rd(&c, M6522_REG_T2CL);
    CHECK(!(c.ifr & M6522_IRQ_T2));
    // ---- T2 pulse counting on PB6: 3 falling edges expire N=2
    m6522_reset(&c); in_pins = M6522_PB6;
    wr(&c, M6522_REG_ACR, 0x20);
    wr(&c, M6522_REG_T2CL, 2); wr(&c, M6522_REG_T2CH, 0);
    for (int i = 0; i < 4; i++) { in_pins = 0; idle(&c); in_pins = M6522_PB6; idle(&c); }
    CHECK(c.ifr & M6522_IRQ_T2);
    in_pins = 0;
    // ---- IER set/clear semantic, IFR clear by write
    m6522_reset(&c);
    wr(&c, M6522_REG_IER, 0xFF); CHECK((rd(&c, M6522_REG_IER) & 0x7F) == 0x7F);
    wr(&c, M6522_REG_IER, 0x42); CHECK((rd(&c, M6522_REG_IER) & 0x7F) == 0x3D);
    c.ifr = 0x7F; wr(&c, M6522_REG_IFR, 0x21); CHECK(c.ifr == 0x5E);
    // ---- CA1 flag, cleared by reading port A; CA2 handshake
    m6522_reset(&c); in_pins = 0;
    wr(&c, M6522_REG_PCR, 0x01);                    // CA1 positive edge, CA2 negative-edge input
    wr(&c, M6522_REG_IER, 0x80 | M6522_IRQ_CA1);
    in_pins = M6522_CA1; p = idle(&c);
    CHECK(p & M6522_IRQ); CHECK(c.ifr & M6522_IRQ_CA1);
    rd(&c, M6522_REG_ORA); p = idle(&c);
    CHECK(!(p & M6522_IRQ));
    m6522_reset(&c); in_pins = 0;
    wr(&c, M6522_REG_PCR, 0x0A);                    // CA2 handshake output, CA1 negative... (bits 3..1 = 101: pulse)
    p = idle(&c);
    wr(&c, M6522_REG_PCR, 0x08);                    // handshake (100)
    p = idle(&c); CHECK(p & M6522_CA2);
    p = idle(&c); in_pins = 0;
    { uint64_t q = (in_pins & ~M6522_RS) | M6522_CS | M6522_RW | M6522_REG_ORA; m6522_tick(&c, q); }
    p = idle(&c); CHECK(!(p & M6522_CA2));
    in_pins = M6522_CA1; wr(&c, M6522_REG_PCR, 0x08 | 0x01); // active edge setup (rising)
    in_pins = 0; idle(&c); in_pins = M6522_CA1; p = idle(&c); CHECK(p & M6522_CA2);
    in_pins = 0;
    // ---- manual CA2 / CB2 outputs
    m6522_reset(&c);
    wr(&c, M6522_REG_PCR, 0xCC);                    // CA2 low (110), CB2 low (110)
    p = idle(&c); CHECK(!(p & M6522_CA2) && !(p & M6522_CB2));
    wr(&c, M6522_REG_PCR, 0xEE);                    // both high
    p = idle(&c); CHECK((p & M6522_CA2) && (p & M6522_CB2));
    // ---- port A / B with DDR
    m6522_reset(&c); in_pins = 0;
    wr(&c, M6522_REG_DDRA, 0x0F); wr(&c, M6522_REG_ORA, 0x05);
    M6522_SET_PAB(in_pins, 0xA0, 0x00);
    CHECK(rd(&c, M6522_REG_ORA2) == 0xA5);
    wr(&c, M6522_REG_DDRB, 0xFF); wr(&c, M6522_REG_ORB, 0x3C);
    CHECK(rd(&c, M6522_REG_ORB) == 0x3C);
    // ---- latching of port A on CA1 active edge
    m6522_reset(&c); in_pins = 0;
    wr(&c, M6522_REG_ACR, 0x01); wr(&c, M6522_REG_PCR, 0x01);
    M6522_SET_PAB(in_pins, 0x11, 0); idle(&c);
    in_pins |= M6522_CA1; idle(&c);                 // latches 0x11
    M6522_SET_PAB(in_pins, 0x22, 0); idle(&c);
    CHECK(rd(&c, M6522_REG_ORA2) == 0x11);
    in_pins = 0;
    // ---- shift out under Phi2 (mode 110): 8 falling CB1 edges, bits MSB first
    m6522_reset(&c); in_pins = 0;
    wr(&c, M6522_REG_ACR, 0x18);                    // 110
    wr(&c, M6522_REG_SR, 0xA5);
    int bits[8], nb = 0, prev_cb1 = 1;
    for (int i = 0; i < 40 && nb < 8; i++) {
        p = idle(&c); int cb1 = (p >> M6522_PIN_CB1) & 1;
        if (prev_cb1 && !cb1) bits[nb++] = (p >> M6522_PIN_CB2) & 1;
        prev_cb1 = cb1;
    }
    CHECK(nb == 8);
    { int exp[8] = {1,0,1,0,0,1,0,1}; for (int i = 0; i < 8; i++) CHECK(bits[i] == exp[i]); }
    for (int i = 0; i < 10; i++) idle(&c);
    CHECK(c.ifr & M6522_IRQ_SR);
    // ---- free-run shift under T2 (mode 100): CB2 square wave (PET sound), period set by T2
    m6522_reset(&c); in_pins = 0;
    wr(&c, M6522_REG_T2CL, 3); wr(&c, M6522_REG_T2CH, 0);
    wr(&c, M6522_REG_ACR, 0x10);                    // 100
    wr(&c, M6522_REG_SR, 0xF0);
    int edges = 0; int lastcb2 = -1;
    for (int i = 0; i < 400; i++) { p = idle(&c); int b = (p >> M6522_PIN_CB2) & 1; if (lastcb2 >= 0 && b != lastcb2) edges++; lastcb2 = b; }
    CHECK(edges > 10);
    // ---- shift in under external clock (mode 011)
    m6522_reset(&c); in_pins = 0;
    wr(&c, M6522_REG_ACR, 0x0C);                    // 011
    rd(&c, M6522_REG_SR);                           // start
    { int d[8] = {1,1,0,0,1,0,1,0};
      for (int i = 0; i < 8; i++) {
          in_pins = d[i] ? M6522_CB2 : 0; idle(&c);
          in_pins |= M6522_CB1; idle(&c);           // rising edge samples
          in_pins &= ~M6522_CB1; idle(&c);
      }
    }
    CHECK(c.sr == 0xCA);
    CHECK(c.ifr & M6522_IRQ_SR);
    printf(fails ? "m6522: %d FAILED\n" : "m6522: all ok\n", fails);
    return fails != 0;
}
