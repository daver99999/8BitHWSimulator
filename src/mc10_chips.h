#pragma once
/* mc10_chips.h - the chips of the Tandy MC-10, see mc10_chips.c (the 6847 is the one of coco_chips.c, PT_6847_MC10). */
#include "parts.h"
#include "../chips/m6803.h"

extern const part_type_t PT_6803, PT_XTAL_MC10, PT_74LS155, PT_74LS367, PT_74LS76, PT_MC14503, PT_TMS2564;

/* The CPU core of an MC6803 part (registers, for the monitor and for tests). */
const m6803_t *mc10_cpu_core(sim_t *s, int part);
