#pragma once
/* m6803.h - Motorola MC6803 (6801 without the ROM) CPU core with its on-chip RAM, ports, timer and SCI status,
 * one E cycle per call. Written for this simulator, in the style of m6809.h: a coroutine that runs the program until
 * it needs the bus, presents the address (and the data of a write) and returns; the next call gets the byte the bus
 * carried and goes on. Every instruction has the cycle count of the real chip (the unused cycles are idle reads of $FFFF).
 *
 * The chip runs in the expanded multiplexed mode: the registers ($00-$1F) and the 128 bytes of RAM ($80-$FF) are inside and a
 * cycle that touches them does not use the bus (c->ext is 0 then); the other addresses are on the pins.
 *
 *     m6803_cycle(&cpu, data_read, p1_pins, p2_pins, nmi_n, irq_n, reset_n);
 *     -> cpu.addr, cpu.rw (1: read), cpu.dout (a write's data), cpu.ext (the cycle uses the bus)
 *        cpu.ddr1/cpu.p1 and cpu.ddr2/cpu.p2: the port outputs (the pins are driven where the DDR bit is 1).
 */
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t pc, x, s;
    uint8_t a, b, cc;
    uint16_t addr;
    uint8_t dout, rw, ext;
    uint8_t din, idin;
    /* on chip */
    uint8_t ram[128];
    uint8_t ddr1, ddr2, p1, p2;     /* ports: data direction and output latch */
    uint8_t p1_pins, p2_pins;       /* the pins, at the cycle */
    uint8_t tcsr, trcsr, rmcr, ramctl;
    uint16_t counter, ocr, icr, cnt_lo_latch;
    uint8_t tcsr_read, tdr, rdr;
    /* inside */
    int line;
    uint8_t op, mode, kind, target, nc, nmi_prev, nmi_pend, need_reset, irqkind, wait;
    uint8_t in_irq, in_nmi;
    uint16_t ea, val, tmp, res;
    uint32_t cycles;
} m6803_t;

#define M6803_C 0x01
#define M6803_V 0x02
#define M6803_Z 0x04
#define M6803_N 0x08
#define M6803_I 0x10
#define M6803_H 0x20

void m6803_init(m6803_t *c);
void m6803_cycle(m6803_t *c, uint8_t din, uint8_t p1_pins, uint8_t p2_pins, bool nmi_n, bool irq_n, bool reset_n);
