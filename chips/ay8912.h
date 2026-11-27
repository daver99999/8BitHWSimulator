#pragma once
/*#
    # ay8912.h

    General Instrument AY-3-8912 (PSG) core, as used in the Timex Sinclair
    TS2068: three square wave channels, a noise generator, an envelope
    generator and a mixer, 16 registers, one 8 bit I/O port (A).
    The 8912 has no port B and no second chip select; BC2 is tied high.

    Written for this project from the public AY-3-8910/8912 datasheet and the
    well known behaviour of the chip (the tone counters divide the clock by 8, noise and envelope by 16,
    the 17 bit LFSR noise with taps 0 and 3, the 16 step envelope, the
    logarithmic DAC levels). No code from other emulators.

    Bus protocol, as on the TS2068 (BDIR, BC1 with BC2 high):
        BDIR BC1
         0    0   inactive
         0    1   read the selected register
         1    0   write the selected register
         1    1   latch the register address
    Port A: bits written to register 14 drive the pins when the port is set to
    output (register 7 bit 6), otherwise the pins are read.
#*/
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t reg[16];
    uint8_t addr;               /* the selected register */
    /* Divider: the chip is clocked at f; the tone counters count f/8, noise and envelope f/16. */
    uint8_t div;                /* 0..15 */
    uint16_t tone_cnt[3];
    bool tone_out[3];
    uint8_t noise_cnt;
    uint32_t lfsr;              /* 17 bit */
    bool noise_out;
    uint16_t env_cnt;
    int8_t env_step;            /* 0..15 */
    bool env_hold, env_up, env_alt;
    uint8_t env_vol;            /* 0..15 */
    uint8_t port_a_in;          /* what the pins show when the port is an input */
    float out[3];               /* channel levels 0..1 */
} ay8912_t;

#define AY_BUS_NONE    0
#define AY_BUS_READ    1
#define AY_BUS_WRITE   2
#define AY_BUS_ADDR    3

void ay8912_reset(ay8912_t *a);
/* One cycle of the clock pin (TS2068: 1.764 MHz = CPU clock / 2). */
void ay8912_tick(ay8912_t *a);
/* A bus cycle: mode is AY_BUS_*; data is the byte on D0-D7 for write and address. For
 * read the register is returned. */
uint8_t ay8912_bus(ay8912_t *a, int mode, uint8_t data);
/* The level of port A's output pins (1 = high; pins read back what is driven
 * when the port is an output, else the input pins with the pull-ups). */
uint8_t ay8912_port_a_pins(const ay8912_t *a);
/* Mixed mono output, 0..1 */
float ay8912_output(const ay8912_t *a);

#ifdef CHIPS_IMPL
static const float ay_dac[16] = {   /* the amplitude table of the datasheet, normalised */
    0.0000f, 0.0100f, 0.0145f, 0.0211f, 0.0307f, 0.0455f, 0.0645f, 0.1073f,
    0.1266f, 0.2050f, 0.2922f, 0.3728f, 0.4925f, 0.6353f, 0.8056f, 1.0000f };
static const uint8_t ay_mask[16] = { 0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0x1F,0xFF,0x1F,0x1F,0x1F,0xFF,0xFF,0x0F,0xFF,0xFF };

void ay8912_reset(ay8912_t *a) {
    for (int i = 0; i < 16; i++) a->reg[i] = 0;
    a->addr = 0; a->div = 0; a->lfsr = 1; a->noise_cnt = 0; a->noise_out = true;
    for (int i = 0; i < 3; i++) { a->tone_cnt[i] = 0; a->tone_out[i] = false; a->out[i] = 0; }
    a->env_cnt = 0; a->env_step = 0; a->env_hold = false; a->env_up = false; a->env_alt = false; a->env_vol = 0;
    a->port_a_in = 0xFF;
}

static void ay_env_restart(ay8912_t *a) {
    uint8_t shape = a->reg[13] & 15;
    a->env_cnt = 0; a->env_step = 0; a->env_hold = false;
    a->env_up = (shape & 4) != 0;               /* attack */
    a->env_vol = a->env_up ? 0 : 15;
}

static void ay_env_step(ay8912_t *a) {
    if (a->env_hold) return;
    uint8_t shape = a->reg[13] & 15;
    if (++a->env_step < 16) { a->env_vol = a->env_up ? a->env_step : 15 - a->env_step; return; }
    /* end of a ramp */
    if (!(shape & 8)) {                         /* shapes 0-7: one ramp, then silence (4-7: low) */
        a->env_hold = true; a->env_vol = 0; return;
    }
    if (shape & 1) {                            /* hold */
        a->env_hold = true;
        bool end_high = (shape & 2) ? !a->env_up : a->env_up;   /* alternate flips the last level */
        a->env_vol = end_high ? 15 : 0;
        return;
    }
    a->env_step = 0;                            /* repeat, alternating or not */
    if (shape & 2) a->env_up = !a->env_up;
    a->env_vol = a->env_up ? 0 : 15;
}

void ay8912_tick(ay8912_t *a) {
    /* the tone counters run at f/8 and toggle at the period: the pitch is f/(16 x period); noise and envelope run at f/16 */
    ++a->div;
    bool full = a->div >= 16;
    if (a->div != 8 && !full) goto level;
    if (full) a->div = 0;
    for (int c = 0; c < 3; c++) {
        uint16_t period = (uint16_t)(a->reg[c * 2] | (a->reg[c * 2 + 1] & 15) << 8);
        if (period == 0) period = 1;
        if (++a->tone_cnt[c] >= period) { a->tone_cnt[c] = 0; a->tone_out[c] = !a->tone_out[c]; }
    }
    if (!full) goto level;
    /* noise: every period steps, shifted every 2 of them */
    uint8_t np = a->reg[6] & 31; if (np == 0) np = 1;
    if (++a->noise_cnt >= np) {
        a->noise_cnt = 0;
        uint32_t bit = (a->lfsr ^ (a->lfsr >> 3)) & 1;
        a->lfsr = (a->lfsr >> 1) | (bit << 16);
        a->noise_out = (a->lfsr & 1) != 0;
    }
    uint16_t ep = (uint16_t)(a->reg[11] | a->reg[12] << 8); if (ep == 0) ep = 1;
    if (++a->env_cnt >= ep) { a->env_cnt = 0; ay_env_step(a); }
level:;
    uint8_t mix = a->reg[7];
    for (int c = 0; c < 3; c++) {
        bool tone = (mix & (1 << c)) ? true : a->tone_out[c];
        bool noise = (mix & (8 << c)) ? true : a->noise_out;
        uint8_t v = a->reg[8 + c];
        uint8_t vol = (v & 16) ? a->env_vol : (v & 15);
        a->out[c] = (tone && noise) ? ay_dac[vol] : 0.0f;
    }
}

uint8_t ay8912_bus(ay8912_t *a, int mode, uint8_t data) {
    switch (mode) {
    case AY_BUS_ADDR: a->addr = data & 15; break;   /* the 8912's addresses above 15 select nothing */
    case AY_BUS_WRITE:
        a->reg[a->addr] = data & ay_mask[a->addr];
        if (a->addr == 13) ay_env_restart(a);
        break;
    case AY_BUS_READ:
        if (a->addr == 14) return (a->reg[7] & 0x40) ? a->reg[14] : a->port_a_in;
        if (a->addr == 15) return 0xFF;             /* no port B on the 8912 */
        return a->reg[a->addr] & ay_mask[a->addr];
    }
    return 0xFF;
}

uint8_t ay8912_port_a_pins(const ay8912_t *a) {
    return (a->reg[7] & 0x40) ? a->reg[14] : a->port_a_in;
}

float ay8912_output(const ay8912_t *a) { return (a->out[0] + a->out[1] + a->out[2]) / 3.0f; }
#endif
