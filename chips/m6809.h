#pragma once
/* m6809.h - Motorola MC6809 / MC6809E CPU core, one bus cycle per call.
 *
 * Written for this simulator (not taken from another emulator). The core is a coroutine: m6809_cycle() runs the
 * program until it needs the bus, presents the address (and the data for a write) in the struct and returns; the
 * next call gets the byte the bus carried (for a read) and goes on. Every call is one cycle of the E clock, so the
 * instruction timing is that of the real chip: an instruction that does fewer bus accesses than its cycle count is
 * padded with idle cycles (address $FFFF, VMA low) the way the real chip has internal cycles.
 *
 * Usage (once per E cycle, at the end of the cycle, when the bus data is valid):
 *     m6809_cycle(&cpu, data_read, nmi_n, firq_n, irq_n, reset_n, halt_n);
 *     -> cpu.addr, cpu.rw (1: read), cpu.dout (a write's data), cpu.avma (the cycle is a valid memory cycle),
 *        cpu.lic (the cycle is the last one of an instruction), cpu.ba / cpu.bs (bus available / status).
 */
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    /* registers */
    uint16_t pc, x, y, u, s;
    uint8_t a, b, dp, cc;
    /* the bus of the coming cycle */
    uint16_t addr;
    uint8_t dout, rw, avma, lic, ba, bs;
    uint8_t din;                    /* the byte the bus carried in the last cycle */
    /* inside */
    int line;                       /* where the coroutine goes on */
    uint8_t op, page, post, kind, mode, opsz, nc, target, nmi_prev, nmi_pend, nmi_armed, need_reset, wait;
    uint8_t irqkind, halted, ret, ind, nfetch, pcrel;
    uint8_t in_nmi, in_firq, in_irq, in_halt;
    uint16_t ea, val, tmp, res;
    uint8_t regs;                   /* PSH/PUL postbyte */
    uint32_t cycles;
} m6809_t;

#define M6809_C 0x01
#define M6809_V 0x02
#define M6809_Z 0x04
#define M6809_N 0x08
#define M6809_I 0x10
#define M6809_H 0x20
#define M6809_F 0x40
#define M6809_E 0x80

void m6809_init(m6809_t *c);
void m6809_cycle(m6809_t *c, uint8_t din, bool nmi_n, bool firq_n, bool irq_n, bool reset_n, bool halt_n);
