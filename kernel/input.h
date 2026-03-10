#ifndef DRAGOON_INPUT_H
#define DRAGOON_INPUT_H

#include "types.h"

/* Key codes */
#define KEY_NONE   0
#define KEY_UP     128
#define KEY_DOWN   129
#define KEY_LEFT   130
#define KEY_RIGHT  131
#define KEY_ENTER  10
#define KEY_ESC    27
#define KEY_SPACE  32
#define KEY_BACKSPACE 127

/* Mouse button flags */
#define MOUSE_BTN_LEFT   1
#define MOUSE_BTN_RIGHT  2

/* Initialize input subsystem (probes virtio devices) */
void input_init(void);

/* Non-blocking key read. Returns KEY_NONE if no key available.
 * Checks virtio keyboard first, then falls back to UART. */
int input_poll(void);

/* Get current mouse position and button state */
void mouse_get_state(int *x, int *y, int *buttons);

#endif /* DRAGOON_INPUT_H */
