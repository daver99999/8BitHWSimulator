/* m6803.c - the MC6803 core (see chips/m6803.h): a coroutine that does one bus cycle per call, like m6809.c.
 * Y() leaves the function and the next call comes back to the same line through the switch (c->line). No yield may sit
 * inside a nested switch: the code between the yields uses if chains and gotos, the helpers never yield. */
#include "m6803.h"
#include <string.h>

enum { M_INH, M_IMM8, M_IMM16, M_DIR, M_IDX, M_EXT, M_REL };
enum { K_NOP, K_RD8, K_LD16, K_CP16, K_ST8, K_ST16, K_RMW, K_TST, K_JMP, K_JSR, K_SPECIAL };

static void set_flag(m6803_t *c, uint8_t f, bool on) { if (on) c->cc |= f; else c->cc &= (uint8_t)~f; }
static void set_nz8(m6803_t *c, uint8_t r) { set_flag(c, M6803_N, r & 0x80); set_flag(c, M6803_Z, r == 0); }
static void set_nz16(m6803_t *c, uint16_t r) { set_flag(c, M6803_N, r & 0x8000); set_flag(c, M6803_Z, r == 0); }

static uint8_t add8(m6803_t *c, uint8_t a, uint8_t b, unsigned cin) {
    unsigned r = (unsigned)a + b + cin;
    set_flag(c, M6803_H, ((a & 0xF) + (b & 0xF) + cin) > 0xF);
    set_flag(c, M6803_C, r > 0xFF);
    set_flag(c, M6803_V, ((a ^ r) & (b ^ r) & 0x80) != 0);
    set_nz8(c, (uint8_t)r);
    return (uint8_t)r;
}
static uint8_t sub8(m6803_t *c, uint8_t a, uint8_t b, unsigned cin) {
    int r = (int)a - b - (int)cin;
    set_flag(c, M6803_C, r < 0);
    set_flag(c, M6803_V, ((a ^ b) & (a ^ r) & 0x80) != 0);
    set_nz8(c, (uint8_t)r);
    return (uint8_t)r;
}
static uint16_t add16(m6803_t *c, uint16_t a, uint16_t b) {
    uint32_t r = (uint32_t)a + b;
    set_flag(c, M6803_C, r > 0xFFFF);
    set_flag(c, M6803_V, ((a ^ r) & (b ^ r) & 0x8000) != 0);
    set_nz16(c, (uint16_t)r);
    return (uint16_t)r;
}
static uint16_t sub16(m6803_t *c, uint16_t a, uint16_t b, bool keep_c) {
    int32_t r = (int32_t)a - b;
    if (!keep_c) set_flag(c, M6803_C, r < 0);
    set_flag(c, M6803_V, ((a ^ b) & (a ^ r) & 0x8000) != 0);
    set_nz16(c, (uint16_t)r);
    return (uint16_t)r;
}

/* NEG COM LSR ROR ASR ASL ROL DEC INC TST CLR by the low nibble of the opcode */
static uint8_t unary(m6803_t *c, int which, uint8_t v) {
    uint8_t r = v;
    bool cin = c->cc & M6803_C;
    switch (which) {
    case 0x0: r = (uint8_t)(0 - v); set_flag(c, M6803_C, r != 0); set_flag(c, M6803_V, v == 0x80); break;
    case 0x3: r = (uint8_t)~v; set_flag(c, M6803_C, true); set_flag(c, M6803_V, false); break;
    case 0x4: r = v >> 1; set_flag(c, M6803_C, v & 1); break;
    case 0x6: r = (uint8_t)((cin << 7) | (v >> 1)); set_flag(c, M6803_C, v & 1); break;
    case 0x7: r = (uint8_t)((v >> 1) | (v & 0x80)); set_flag(c, M6803_C, v & 1); break;
    case 0x8: r = (uint8_t)(v << 1); set_flag(c, M6803_C, v & 0x80); break;
    case 0x9: r = (uint8_t)((v << 1) | cin); set_flag(c, M6803_C, v & 0x80); break;
    case 0xA: r = (uint8_t)(v - 1); set_flag(c, M6803_V, v == 0x80); break;
    case 0xC: r = (uint8_t)(v + 1); set_flag(c, M6803_V, v == 0x7F); break;
    case 0xD: set_flag(c, M6803_C, false); set_flag(c, M6803_V, false); break;
    case 0xF: r = 0; set_flag(c, M6803_C, false); set_flag(c, M6803_V, false); break;
    }
    set_nz8(c, r);
    if (which == 0x4 || which == 0x6 || which == 0x7 || which == 0x8 || which == 0x9)
        set_flag(c, M6803_V, ((c->cc & M6803_N) != 0) != ((c->cc & M6803_C) != 0));
    return r;
}
static bool unary_valid(int lo) { return lo == 0 || lo == 3 || lo == 4 || lo == 6 || lo == 7 || lo == 8 || lo == 9 || lo == 0xA || lo == 0xC || lo == 0xD || lo == 0xF; }

static bool cond(const m6803_t *c, int cc) {
    bool C = c->cc & M6803_C, V = c->cc & M6803_V, Z = c->cc & M6803_Z, N = c->cc & M6803_N;
    switch (cc & 0xF) {
    case 0: return true;
    case 1: return false;
    case 2: return !(C || Z);
    case 3: return C || Z;
    case 4: return !C;
    case 5: return C;
    case 6: return !Z;
    case 7: return Z;
    case 8: return !V;
    case 9: return V;
    case 10: return !N;
    case 11: return N;
    case 12: return N == V;
    case 13: return N != V;
    case 14: return !Z && N == V;
    default: return Z || N != V;
    }
}

/* the opcode's addressing mode, kind and cycle count */
static void decode(m6803_t *c) {
    uint8_t op = c->op, hi = op >> 4, lo = op & 0xF;
    c->mode = M_INH; c->kind = K_NOP; c->target = 1;
    if (hi == 0) {
        c->kind = K_SPECIAL;
        c->target = (op == 0x19) ? 2 : 1;
        return;
    }
    if (hi == 1) { c->kind = (lo == 0 || lo == 1 || lo == 6 || lo == 7 || lo == 9 || lo == 0xB) ? K_SPECIAL : K_NOP; c->target = (op == 0x19) ? 2 : 1; return; }
    if (hi == 2) { c->kind = K_SPECIAL; c->mode = M_REL; c->target = 3; return; }
    if (hi == 3) {
        c->kind = K_SPECIAL;
        static const uint8_t t3[16] = { 1, 1, 3, 3, 1, 1, 4, 4, 5, 5, 1, 10, 4, 10, 9, 12 };
        c->target = t3[lo];
        return;
    }
    if (hi == 4 || hi == 5) { c->kind = unary_valid(lo) ? K_SPECIAL : K_NOP; return; }
    if (hi == 6 || hi == 7) {
        c->mode = (hi == 6) ? M_IDX : M_EXT;
        if (lo == 0xE) { c->kind = K_JMP; c->target = 3; }
        else if (!unary_valid(lo)) { c->kind = K_NOP; c->mode = M_INH; }
        else if (lo == 0xD) { c->kind = K_TST; c->target = 4; }
        else { c->kind = K_RMW; c->target = 6; }
        return;
    }
    /* 8x..Fx */
    c->mode = (hi == 8 || hi == 0xC) ? M_IMM8 : (hi == 9 || hi == 0xD) ? M_DIR : (hi == 0xA || hi == 0xE) ? M_IDX : M_EXT;
    int m = c->mode;
    bool bcol = hi >= 0xC;
    if (!bcol && lo == 0xD) {
        if (hi == 8) { c->kind = K_SPECIAL; c->mode = M_REL; c->target = 6; }
        else { c->kind = K_JSR; c->target = (m == M_EXT) ? 6 : 5; }
        return;
    }
    /* the 8 bit operations, A or B */
    static const bool rd8[16] = { 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0 };
    if (!bcol) {
        if (lo <= 0xB && rd8[lo]) { c->kind = K_RD8; c->target = (m == M_IMM8) ? 2 : (m == M_DIR) ? 3 : 4; return; }
        if (lo == 3) { c->kind = K_CP16; c->mode = (m == M_IMM8) ? M_IMM16 : m; c->target = (m == M_IMM8) ? 4 : (m == M_DIR) ? 5 : 6; return; }
        if (lo == 7 && m != M_IMM8) { c->kind = K_ST8; c->target = (m == M_DIR) ? 3 : 4; return; }
        if (lo == 0xC) { c->kind = K_CP16; c->mode = (m == M_IMM8) ? M_IMM16 : m; c->target = (m == M_IMM8) ? 4 : (m == M_DIR) ? 5 : 6; return; }
        if (lo == 0xE) { c->kind = K_LD16; c->mode = (m == M_IMM8) ? M_IMM16 : m; c->target = (m == M_IMM8) ? 3 : (m == M_DIR) ? 4 : 5; return; }
        if (lo == 0xF && m != M_IMM8) { c->kind = K_ST16; c->target = (m == M_DIR) ? 4 : 5; return; }
        c->kind = K_NOP; c->mode = M_INH;
        return;
    }
    if (lo <= 0xB && rd8[lo] && lo != 3) { c->kind = K_RD8; c->target = (m == M_IMM8) ? 2 : (m == M_DIR) ? 3 : 4; return; }
    if (lo == 3) { c->kind = K_CP16; c->mode = (m == M_IMM8) ? M_IMM16 : m; c->target = (m == M_IMM8) ? 4 : (m == M_DIR) ? 5 : 6; return; }   /* ADDD */
    if (lo == 7 && m != M_IMM8) { c->kind = K_ST8; c->target = (m == M_DIR) ? 3 : 4; return; }
    if (lo == 0xC || lo == 0xE) { c->kind = K_LD16; c->mode = (m == M_IMM8) ? M_IMM16 : m; c->target = (m == M_IMM8) ? 3 : (m == M_DIR) ? 4 : 5; return; }
    if ((lo == 0xD || lo == 0xF) && m != M_IMM8) { c->kind = K_ST16; c->target = (m == M_DIR) ? 4 : 5; return; }
    c->kind = K_NOP; c->mode = M_INH;
}

/* the on-chip registers ($00-$14) and RAM ($80-$FF) */
static bool is_internal(const m6803_t *c, uint16_t a) {
    if (a <= 0x03 || (a >= 0x08 && a <= 0x14)) return true;
    return a >= 0x80 && a <= 0xFF && (c->ramctl & 0x40);
}

static uint8_t int_read(m6803_t *c, uint16_t a) {
    if (a >= 0x80) return c->ram[a - 0x80];
    switch (a) {
    case 0x00: return 0xFF;
    case 0x01: return 0xFF;
    case 0x02: return (uint8_t)((c->p1 & c->ddr1) | (c->p1_pins & ~c->ddr1));
    case 0x03: return (uint8_t)(0xE0 | (((c->p2 & c->ddr2) | (c->p2_pins & ~c->ddr2)) & 0x1F));
    case 0x08: c->tcsr_read = 1; return c->tcsr;
    case 0x09:
        if (c->tcsr_read) c->tcsr &= (uint8_t)~0x20;      /* TOF cleared by TCSR then the counter */
        c->cnt_lo_latch = (uint16_t)(c->counter & 0xFF);
        return (uint8_t)(c->counter >> 8);
    case 0x0A: return (uint8_t)c->cnt_lo_latch;
    case 0x0B: return (uint8_t)(c->ocr >> 8);
    case 0x0C: return (uint8_t)c->ocr;
    case 0x0D: if (c->tcsr_read) c->tcsr &= (uint8_t)~0x80; return (uint8_t)(c->icr >> 8);
    case 0x0E: return (uint8_t)c->icr;
    case 0x10: return c->rmcr;
    case 0x11: return (uint8_t)(c->trcsr | 0x20);          /* the transmitter is always ready */
    case 0x12: return c->rdr;
    case 0x13: return c->tdr;
    case 0x14: return c->ramctl;
    }
    return 0xFF;
}

static void int_write(m6803_t *c, uint16_t a, uint8_t d) {
    if (a >= 0x80) { c->ram[a - 0x80] = d; return; }
    switch (a) {
    case 0x00: c->ddr1 = d; break;
    case 0x01: c->ddr2 = d & 0x1F; break;
    case 0x02: c->p1 = d; break;
    case 0x03: c->p2 = d & 0x1F; break;
    case 0x08: c->tcsr = (uint8_t)((c->tcsr & 0xE0) | (d & 0x1F)); break;
    case 0x09: c->counter = 0xFFF8; break;
    case 0x0A: c->counter = 0xFFF8; break;
    case 0x0B: c->ocr = (uint16_t)((c->ocr & 0xFF) | (d << 8)); if (c->tcsr_read) c->tcsr &= (uint8_t)~0x40; break;
    case 0x0C: c->ocr = (uint16_t)((c->ocr & 0xFF00) | d); if (c->tcsr_read) c->tcsr &= (uint8_t)~0x40; break;
    case 0x10: c->rmcr = d; break;
    case 0x11: c->trcsr = d; break;
    case 0x13: c->tdr = d; break;
    case 0x14: c->ramctl = (uint8_t)((c->ramctl & 0x80) | (d & 0x40) | (d & 0x80)); break;
    }
}

void m6803_init(m6803_t *c) {
    memset(c, 0, sizeof(*c));
    c->need_reset = 1;
    c->cc = 0xC0 | M6803_I;
    c->addr = 0xFFFF; c->rw = 1; c->ext = 1;
    c->nmi_prev = 1;
    c->ocr = 0xFFFF;
    c->ramctl = 0xC0;
    c->tcsr_read = 0;
}

#define Y_(n)    do { c->line = (n); c->nc++; c->cycles++; return; case (n):; } while (0)
#define Y()      Y_(__COUNTER__ + 1)
#define RD(a)    do { c->addr = (uint16_t)(a); c->rw = 1; c->ext = !is_internal(c, c->addr); \
                      c->idin = c->ext ? 0 : int_read(c, c->addr); Y(); if (!c->ext) c->din = c->idin; } while (0)
#define WR(a, d) do { c->addr = (uint16_t)(a); c->dout = (uint8_t)(d); c->rw = 0; c->ext = !is_internal(c, c->addr); \
                      if (!c->ext) { int_write(c, c->addr, c->dout); } Y(); } while (0)
#define IDLE()   do { c->addr = 0xFFFF; c->rw = 1; c->ext = 1; Y(); } while (0)
#define FETCH()  do { RD(c->pc); c->pc++; } while (0)
#define PUSH8(v) do { WR(c->s, (v)); c->s--; } while (0)
#define PULL8()  do { c->s++; RD(c->s); } while (0)

/* the interrupt vector that is due, or 0 */
static uint16_t due_vector(m6803_t *c) {
    if (c->nmi_pend) { c->nmi_pend = 0; return 0xFFFC; }
    if (c->cc & M6803_I) return 0;
    if (c->in_irq) return 0xFFF8;
    if ((c->tcsr & 0x80) && (c->tcsr & 0x10)) return 0xFFF6;
    if ((c->tcsr & 0x40) && (c->tcsr & 0x08)) return 0xFFF4;
    if ((c->tcsr & 0x20) && (c->tcsr & 0x04)) return 0xFFF2;
    return 0;
}

void m6803_cycle(m6803_t *c, uint8_t din, uint8_t p1_pins, uint8_t p2_pins, bool nmi_n, bool irq_n, bool reset_n) {
    c->din = din;
    c->p1_pins = p1_pins; c->p2_pins = p2_pins;
    c->in_irq = !irq_n;
    if (c->nmi_prev && !nmi_n) c->nmi_pend = 1;
    c->nmi_prev = nmi_n;
    /* the timer: one count per E cycle */
    c->counter++;
    if (c->counter == 0) c->tcsr |= 0x20;
    if (c->counter == c->ocr) c->tcsr |= 0x40;
    if (!reset_n) {
        c->need_reset = 1; c->line = 0; c->addr = 0xFFFF; c->rw = 1; c->ext = 1; c->wait = 0;
        c->cc |= M6803_I; c->ddr1 = c->ddr2 = c->p1 = c->p2 = 0; c->tcsr = 0; c->counter = 0xFFF8; c->ocr = 0xFFFF;
        c->trcsr = 0; c->ramctl = (uint8_t)(c->ramctl | 0x40); c->nmi_pend = 0;
        return;
    }
    switch (c->line) {
    case 0:
    for (;;) {
        if (c->need_reset) {
            c->need_reset = 0;
            IDLE(); IDLE(); IDLE();
            RD(0xFFFE); c->tmp = (uint16_t)(c->din << 8);
            RD(0xFFFF); c->pc = (uint16_t)(c->tmp | c->din);
        }
        /* ---- between two instructions ---- */
        c->nc = 0;
        c->irqkind = 0;
        c->res = due_vector(c);
        if (c->wait) {
            if (!c->res) { IDLE(); c->nc = 0; continue; }
            c->wait = 0;
            c->tmp = c->res;
            goto vector;
        }
        if (c->res) {
            c->tmp = c->res;
            IDLE(); IDLE();
            goto push_all;
        }
        /* ---- fetch and decode ---- */
        FETCH(); c->op = c->din;
        decode(c);
        c->ea = 0; c->val = 0;
        if (c->mode == M_IMM8) { FETCH(); c->val = c->din; }
        else if (c->mode == M_IMM16) { FETCH(); c->val = (uint16_t)(c->din << 8); FETCH(); c->val |= c->din; }
        else if (c->mode == M_DIR) { FETCH(); c->ea = c->din; }
        else if (c->mode == M_EXT) { FETCH(); c->tmp = (uint16_t)(c->din << 8); FETCH(); c->ea = (uint16_t)(c->tmp | c->din); }
        else if (c->mode == M_IDX) { FETCH(); c->ea = (uint16_t)(c->x + c->din); IDLE(); }
        /* ---- the operation ---- */
        if (c->kind == K_RD8) {
            if (c->mode != M_IMM8) { RD(c->ea); c->val = c->din; }
            {
                uint8_t *r = (c->op >= 0xC0) ? &c->b : &c->a;
                uint8_t v = (uint8_t)c->val;
                switch (c->op & 0xF) {
                case 0x0: *r = sub8(c, *r, v, 0); break;
                case 0x1: sub8(c, *r, v, 0); break;
                case 0x2: *r = sub8(c, *r, v, c->cc & M6803_C); break;
                case 0x4: *r = *r & v; set_nz8(c, *r); set_flag(c, M6803_V, false); break;
                case 0x5: set_nz8(c, *r & v); set_flag(c, M6803_V, false); break;
                case 0x6: *r = v; set_nz8(c, v); set_flag(c, M6803_V, false); break;
                case 0x8: *r = *r ^ v; set_nz8(c, *r); set_flag(c, M6803_V, false); break;
                case 0x9: *r = add8(c, *r, v, c->cc & M6803_C); break;
                case 0xA: *r = *r | v; set_nz8(c, *r); set_flag(c, M6803_V, false); break;
                case 0xB: *r = add8(c, *r, v, 0); break;
                }
            }
        }
        else if (c->kind == K_LD16 || c->kind == K_CP16) {
            if (c->mode != M_IMM16) { RD(c->ea); c->tmp = (uint16_t)(c->din << 8); RD(c->ea + 1); c->val = (uint16_t)(c->tmp | c->din); }
            {
                uint8_t lo = c->op & 0xF;
                uint16_t d = (uint16_t)(c->a << 8 | c->b);
                if (c->kind == K_LD16) {
                    set_nz16(c, c->val); set_flag(c, M6803_V, false);
                    if (lo == 0xC) { c->a = (uint8_t)(c->val >> 8); c->b = (uint8_t)c->val; }      /* LDD */
                    else if ((c->op >> 4) < 0xC) c->s = c->val;                                      /* LDS */
                    else c->x = c->val;                                                              /* LDX */
                } else if (lo == 0xC) sub16(c, c->x, c->val, true);                                  /* CPX */
                else if ((c->op >> 4) < 0xC) { d = sub16(c, d, c->val, false); c->a = (uint8_t)(d >> 8); c->b = (uint8_t)d; }   /* SUBD */
                else { d = add16(c, d, c->val); c->a = (uint8_t)(d >> 8); c->b = (uint8_t)d; }       /* ADDD */
            }
        }
        else if (c->kind == K_ST8) {
            c->val = (c->op >= 0xC0) ? c->b : c->a;
            set_nz8(c, (uint8_t)c->val); set_flag(c, M6803_V, false);
            WR(c->ea, c->val);
        }
        else if (c->kind == K_ST16) {
            uint8_t lo = c->op & 0xF;
            if (lo == 0xD) c->val = (uint16_t)(c->a << 8 | c->b);                                   /* STD */
            else if (lo == 0xF && (c->op >> 4) < 0xC) c->val = c->s;                                 /* STS */
            else c->val = c->x;                                                                      /* STX */
            set_nz16(c, c->val); set_flag(c, M6803_V, false);
            WR(c->ea, c->val >> 8);
            WR(c->ea + 1, c->val & 0xFF);
        }
        else if (c->kind == K_RMW) {
            RD(c->ea);
            c->val = unary(c, c->op & 0xF, c->din);
            IDLE();
            WR(c->ea, c->val);
        }
        else if (c->kind == K_TST) { RD(c->ea); unary(c, 0xD, c->din); }
        else if (c->kind == K_JMP) c->pc = c->ea;
        else if (c->kind == K_JSR) {
            c->res = c->pc; c->pc = c->ea;
            PUSH8(c->res & 0xFF); PUSH8(c->res >> 8);
        }
        else if (c->kind == K_SPECIAL) {
            if (c->mode == M_REL && c->op != 0x8D) {
                FETCH(); c->val = (uint16_t)(int16_t)(int8_t)c->din;
                if (cond(c, c->op)) c->pc = (uint16_t)(c->pc + c->val);
            }
            else if (c->op == 0x8D) {                                              /* BSR */
                FETCH(); c->val = (uint16_t)(int16_t)(int8_t)c->din;
                c->res = c->pc; c->pc = (uint16_t)(c->pc + c->val);
                IDLE();
                PUSH8(c->res & 0xFF); PUSH8(c->res >> 8);
            }
            else if ((c->op >> 4) == 4 || (c->op >> 4) == 5) {                        /* A and B unary */
                uint8_t *r = (c->op & 0x10) ? &c->b : &c->a;
                *r = unary(c, c->op & 0xF, *r);
            }
            else if (c->op == 0x04) {                                              /* LSRD */
                uint16_t d = (uint16_t)(c->a << 8 | c->b);
                set_flag(c, M6803_C, d & 1); d >>= 1;
                set_nz16(c, d); set_flag(c, M6803_V, (c->cc & M6803_C) != 0);
                c->a = (uint8_t)(d >> 8); c->b = (uint8_t)d;
            }
            else if (c->op == 0x05) {                                              /* ASLD */
                uint16_t d = (uint16_t)(c->a << 8 | c->b);
                set_flag(c, M6803_C, d & 0x8000); d = (uint16_t)(d << 1);
                set_nz16(c, d); set_flag(c, M6803_V, ((c->cc & M6803_N) != 0) != ((c->cc & M6803_C) != 0));
                c->a = (uint8_t)(d >> 8); c->b = (uint8_t)d;
            }
            else if (c->op == 0x06) c->cc = (uint8_t)(0xC0 | (c->a & 0x3F));      /* TAP */
            else if (c->op == 0x07) c->a = (uint8_t)(c->cc | 0xC0);                /* TPA */
            else if (c->op == 0x08) { c->x++; set_flag(c, M6803_Z, c->x == 0); }
            else if (c->op == 0x09) { c->x--; set_flag(c, M6803_Z, c->x == 0); }
            else if (c->op == 0x0A) set_flag(c, M6803_V, false);
            else if (c->op == 0x0B) set_flag(c, M6803_V, true);
            else if (c->op == 0x0C) set_flag(c, M6803_C, false);
            else if (c->op == 0x0D) set_flag(c, M6803_C, true);
            else if (c->op == 0x0E) set_flag(c, M6803_I, false);
            else if (c->op == 0x0F) set_flag(c, M6803_I, true);
            else if (c->op == 0x10) c->a = sub8(c, c->a, c->b, 0);                 /* SBA */
            else if (c->op == 0x11) sub8(c, c->a, c->b, 0);                        /* CBA */
            else if (c->op == 0x16) { c->b = c->a; set_nz8(c, c->b); set_flag(c, M6803_V, false); }
            else if (c->op == 0x17) { c->a = c->b; set_nz8(c, c->a); set_flag(c, M6803_V, false); }
            else if (c->op == 0x19) {                                              /* DAA */
                unsigned a = c->a, cf = 0, msn = a & 0xF0, lsn = a & 0x0F;
                if (lsn > 9 || (c->cc & M6803_H)) cf |= 0x06;
                if (msn > 0x90 || (c->cc & M6803_C) || (msn > 0x80 && lsn > 9)) cf |= 0x60;
                a += cf;
                set_flag(c, M6803_C, (c->cc & M6803_C) || a > 0xFF);
                c->a = (uint8_t)a; set_nz8(c, c->a);
            }
            else if (c->op == 0x1B) c->a = add8(c, c->a, c->b, 0);                 /* ABA */
            else if (c->op == 0x30) c->x = (uint16_t)(c->s + 1);                   /* TSX */
            else if (c->op == 0x31) c->s++;
            else if (c->op == 0x34) c->s--;
            else if (c->op == 0x35) c->s = (uint16_t)(c->x - 1);                   /* TXS */
            else if (c->op == 0x3A) c->x = (uint16_t)(c->x + c->b);                /* ABX */
            else if (c->op == 0x32 || c->op == 0x33) { IDLE(); PULL8(); if (c->op == 0x32) c->a = c->din; else c->b = c->din; }
            else if (c->op == 0x36 || c->op == 0x37) { IDLE(); PUSH8(c->op == 0x36 ? c->a : c->b); }
            else if (c->op == 0x38) { IDLE(); PULL8(); c->tmp = (uint16_t)(c->din << 8); PULL8(); c->x = (uint16_t)(c->tmp | c->din); }
            else if (c->op == 0x3C) { IDLE(); PUSH8(c->x & 0xFF); PUSH8(c->x >> 8); }
            else if (c->op == 0x39) { IDLE(); PULL8(); c->tmp = (uint16_t)(c->din << 8); PULL8(); c->pc = (uint16_t)(c->tmp | c->din); }
            else if (c->op == 0x3B) {                                              /* RTI */
                IDLE();
                PULL8(); c->cc = (uint8_t)(c->din | 0xC0);
                PULL8(); c->b = c->din;
                PULL8(); c->a = c->din;
                PULL8(); c->tmp = (uint16_t)(c->din << 8); PULL8(); c->x = (uint16_t)(c->tmp | c->din);
                PULL8(); c->tmp = (uint16_t)(c->din << 8); PULL8(); c->pc = (uint16_t)(c->tmp | c->din);
            }
            else if (c->op == 0x3D) {                                              /* MUL */
                uint16_t d = (uint16_t)(c->a * c->b);
                c->a = (uint8_t)(d >> 8); c->b = (uint8_t)d;
                set_flag(c, M6803_C, c->b & 0x80);
            }
            else if (c->op == 0x3E || c->op == 0x3F) {                                /* WAI, SWI */
                IDLE();
                c->tmp = 0xFFFA;
                if (c->op == 0x3E) c->wait = 1;
                goto push_all;
            }
        }
        /* ---- the rest of the instruction's cycles ---- */
        while (c->nc < c->target) IDLE();
        continue;

    push_all:
        /* the state: PC, X, A, B, CC (the vector is in c->tmp) */
        c->res = c->tmp;
        PUSH8(c->pc & 0xFF); PUSH8(c->pc >> 8);
        PUSH8(c->x & 0xFF); PUSH8(c->x >> 8);
        PUSH8(c->a); PUSH8(c->b); PUSH8(c->cc);
        c->tmp = c->res;
        if (c->wait) continue;
    vector:
        c->cc |= M6803_I;
        c->res = c->tmp;
        RD(c->res); c->tmp = (uint16_t)(c->din << 8);
        RD(c->res + 1); c->pc = (uint16_t)(c->tmp | c->din);
        IDLE();
        continue;
    }
    }
}
