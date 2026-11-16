/* keymap_view.h - the "Keyboard" window: the machine's keys drawn as on the real keyboard, each with
 * the PC key that types it. */
#ifndef KEYMAP_VIEW_H
#define KEYMAP_VIEW_H
#include "ui_common.h"
#include "board.h"

/* Has this machine a keyboard map to show? */
bool keymap_supported(const machine_t *m);
/* Draw the window while *open (the user closing it clears *open). bw, h: the size of the view. */
void keymap_window(struct nk_context *ctx, bool *open, const machine_t *m, float bw, float h);
#endif
