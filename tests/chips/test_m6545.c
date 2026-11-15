#define CHIPS_IMPL
#include "m6545.h"
#include <stdio.h>
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
static void wreg(m6545_t* c, int r, int v) {
    uint64_t p = M6545_CS; M6545_SET_DATA(p, r); m6545_tick(c, p);
    p = M6545_CS | M6545_RS; M6545_SET_DATA(p, v); m6545_tick(c, p);
}
static int rreg(m6545_t* c, int r) {
    uint64_t p = M6545_CS; M6545_SET_DATA(p, r); m6545_tick(c, p);
    p = M6545_CS | M6545_RS | M6545_RW; p = m6545_tick(c, p); return M6545_GET_DATA(p);
}
static int status(m6545_t* c) { uint64_t p = M6545_CS | M6545_RW; p = m6545_tick(c, p); return M6545_GET_DATA(p); }

int main(void) {
    m6545_t c; m6545_init(&c);
    int R[18] = {49, 40, 41, 0x28, 31, 3, 25, 28, 0, 7, 0x00, 0x07, 0, 0, 0, 0};
    for (int i = 0; i < 16; i++) wreg(&c, i, R[i]);
    wreg(&c, 14, 0); wreg(&c, 15, 41);                 // cursor at address 41 (row 1, col 1)
    CHECK(rreg(&c, 15) == 41);
    m6545_reset(&c);
    // run 3 frames and collect statistics
    const int line_chars = 50, frame_lines = 32 * 8 + 3;
    long cycles = 0; int vs_rise = 0, prev_vs = 0, first_vs = -1, second_vs = -1;
    int de_in_line = 0, de_lines_ok = 1, hs_ok = 1, cursor_hits = 0;
    int de_line_count = 0;
    uint16_t first_ma_row1 = 0xFFFF; int vs_len = 0, vs_len_cur = 0, vs_len_ok = 1;
    long total = (long)line_chars * frame_lines * 3;
    int line_no = 0, in_hs = 0;
    for (long i = 0; i < total; i++) {
        uint64_t p = m6545_tick(&c, 0); cycles++;
        int pos = (int)(i % line_chars);
        if (pos == 0 && i) { line_no++; }
        if (p & M6545_DE) de_in_line++;
        if (pos == line_chars - 1) {
            if (de_in_line) { de_line_count++; if (de_in_line != 40) de_lines_ok = 0; }
            de_in_line = 0;
        }
        int hs = (p & M6545_HSYNC) != 0;
        if (hs) in_hs++;
        if (!hs && in_hs) { if (in_hs != 8) hs_ok = 0; in_hs = 0; }
        if (hs && pos != 41 && in_hs == 1) hs_ok = 0;     // starts at R2
        int vs = (p & M6545_VSYNC) != 0;
        if (vs && !prev_vs) { vs_rise++; if (first_vs < 0) first_vs = (int)i; else if (second_vs < 0) second_vs = (int)i; }
        if (vs) vs_len_cur++; else if (vs_len_cur) { vs_len = vs_len_cur; if (vs_len_cur != 2 * line_chars) vs_len_ok = 0; vs_len_cur = 0; }
        prev_vs = vs;
        if (p & M6545_CURSOR) cursor_hits++;
        if ((p & M6545_DE) && M6545_GET_RA(p) == 0 && M6545_GET_MA(p) == 80 && first_ma_row1 == 0xFFFF) first_ma_row1 = 1;
    }
    CHECK(vs_rise == 2 || vs_rise == 3);
    CHECK(second_vs - first_vs == line_chars * frame_lines);      // frame period
    CHECK(de_lines_ok);
    CHECK(vs_len == 2 * line_chars && vs_len_ok);                // VSW=2 lines
    CHECK(de_line_count == 25 * 8 * 3);                           // 25 rows x 8 lines per frame
    CHECK(hs_ok);
    CHECK(first_ma_row1 == 1);
    CHECK(cursor_hits == 3 * 8);                                  // R10=0,R11=7: all 8 lines, once a frame, steady
    // status register: vertical blanking bit set at some point, not always
    int vbl = 0, nvbl = 0;
    for (int i = 0; i < 20000; i++) { int s = status(&c); if (s & 0x20) vbl++; else nvbl++; CHECK(s & 0x80); }
    CHECK(vbl > 0 && nvbl > 0);
    // start address change applies at the next frame
    wreg(&c, 12, 0x01); wreg(&c, 13, 0x00);
    int seen = 0;
    for (long i = 0; i < line_chars * frame_lines * 2 && !seen; i++) {
        uint64_t p = m6545_tick(&c, 0);
        if ((p & M6545_DE) && M6545_GET_MA(p) == 0x0100) seen = 1;
    }
    CHECK(seen);
    // light pen
    for (int i = 0; i < 10; i++) m6545_tick(&c, 0);
    m6545_tick(&c, M6545_LPEN);
    CHECK(status(&c) & 0x40);
    int hi = rreg(&c, 16), lo = rreg(&c, 17);
    CHECK(!(status(&c) & 0x40));
    CHECK((hi | lo) != 0);
    // register read-back: write-only registers read as 0
    CHECK(rreg(&c, 0) == 0);
    // reset holds counters, keeps registers
    m6545_tick(&c, M6545_RES); CHECK(c.reg[0] == 49 && c.hc == 0);
    printf(fails ? "m6545: %d FAILED\n" : "m6545: all ok\n", fails);
    return fails != 0;
}
