/* the PET parts test does not link the Atari chips: parts.c asks whether a ROM part is the Atari's */
#include <stdbool.h>
#include "parts.h"
bool atari_rom_is(const part_t *p) { (void)p; return false; }
