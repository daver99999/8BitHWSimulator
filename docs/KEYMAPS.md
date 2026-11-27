# Keymaps: Atari 800 and Amstrad CPC 464

How the PC keyboard drives the simulated key matrix. Typing a character presses the matrix key that makes it, and presses SHIFT too when the
character needs it (so `"` or `(` just work). Special keys are listed below.

## Atari 800 (J106 matrix, console keys on J114)

| PC key | Atari key | Notes |
|---|---|---|
| letters a-z / A-Z | A-Z | the Atari types capitals; CAPS is a toggle (F7) |
| 0-9 | 0-9 | |
| Shift (left or right) | SHIFT | joins KEYCOM (J106 pin 9) to ground |
| Enter | RETURN | |
| Backspace | DELETE BACK S | |
| Tab | TAB | |
| Esc | ESC | |
| Space | SPACE BAR | |
| F2 | OPTION | J114 pin 2 to ground |
| F3 | SELECT | J114 pin 3 to ground |
| F4 | START | J114 pin 4 to ground |
| F6 | Atari key (inverse video) | |
| F7 | CAPS | |
| the RESET button in the simulator | RESET | pulls ANTIC /RNMI (pin 6) to ground |

Shifted symbols follow the Atari legends:

| Type | Atari key | | Type | Atari key |
|---|---|---|---|---|
| `!` | SHIFT 1 | | `"` | SHIFT 2 |
| `#` | SHIFT 3 | | `$` | SHIFT 4 |
| `%` | SHIFT 5 | | `&` | SHIFT 6 |
| `'` | SHIFT 7 | | `@` | SHIFT 8 |
| `(` | SHIFT 9 | | `)` | SHIFT 0 |
| `<` `>` | their own keys | | `[` `]` | SHIFT `,` and SHIFT `.` |
| `-` `_` | `-` / SHIFT `-` | | `=` `|` | `=` / SHIFT `=` |
| `+` `\` | `+` / SHIFT `+` | | `*` `^` | `*` / SHIFT `*` |
| `;` `:` | `;` / SHIFT `;` | | `/` `?` | `/` / SHIFT `/` |

The cartridge slot is not a key: open `roms/atari800/atari-basic-revc.rom` as a cartridge for BASIC.

**Not mapped yet:** CTRL and BREAK. The 800's cursor movement is CTRL plus `-` `=` `+` `*`, so you cannot move the cursor yet. Neither is on J106 in
this netlist (only the 64 matrix positions, KEYCOM and the console keys are), so adding them needs their wiring checked on the board first.

## Amstrad CPC 464 (J106 keyboard matrix: 10 rows from the 74LS145, 8 columns read through the AY port A)

| PC key | CPC key |
|---|---|
| letters, digits, space | the same key |
| Shift (left or right) | SHIFT |
| Ctrl (left or right) | CTRL |
| Caps Lock | CAPS LOCK |
| Enter | RETURN |
| Keypad Enter | ENTER |
| Backspace or Delete | DEL |
| Esc | ESC |
| Tab | TAB |
| Home | CLR |
| End | COPY |
| Arrow keys | cursor keys |
| F1 to F9 | keypad f1 to f9 |
| F10 | keypad f0 |

Shifted symbols (the CPC's own layout):

| Type | CPC key | | Type | CPC key |
|---|---|---|---|---|
| `!` | SHIFT 1 | | `"` | SHIFT 2 |
| `#` | SHIFT 3 | | `$` | SHIFT 4 |
| `%` | SHIFT 5 | | `&` | SHIFT 6 |
| `'` | SHIFT 7 | | `(` | SHIFT 8 |
| `)` | SHIFT 9 | | `_` | SHIFT 0 |
| `=` | SHIFT `-` | | `\|` | SHIFT `@` |
| `{` | SHIFT `[` | | `}` | SHIFT `]` |
| `+` | SHIFT `;` | | `*` | SHIFT `:` |
| `<` | SHIFT `,` | | `>` | SHIFT `.` |
| `?` | SHIFT `/` | | `` ` `` | SHIFT `\` |

Keys with their own plain character: `^ - @ [ ] \ ; : / . ,`.

**Not mapped:** the joystick (it shares the matrix, row 9) and the keypad digits (the keypad f-keys are the F1 to F10 above).
