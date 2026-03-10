#ifndef DRAGOON_VIRTIO_INPUT_H
#define DRAGOON_VIRTIO_INPUT_H

#include "types.h"

/* Linux evdev event types */
#define EV_SYN 0x00
#define EV_KEY 0x01
#define EV_REL 0x02
#define EV_ABS 0x03

/* Relative axes */
#define REL_X 0x00
#define REL_Y 0x01

/* Absolute axes (for tablet/trackpad) */
#define ABS_X 0x00
#define ABS_Y 0x01

/* Tablet absolute coordinate range (QEMU default) */
#define TABLET_ABS_MAX 32767

/* Mouse buttons (Linux evdev codes) */
#define BTN_LEFT   0x110
#define BTN_RIGHT  0x111
#define BTN_MIDDLE 0x112

/* Virtio input event (8 bytes, evdev format) */
struct virtio_input_event {
    u16 type;
    u16 code;
    u32 value;
} __packed;

/* Initialize virtio input devices (keyboard + mouse) */
int  virtio_input_init(void);

/* Poll for a keyboard event. Returns key code or KEY_NONE. */
int  virtio_kbd_poll(void);

/* Get current mouse state */
void virtio_mouse_poll(int *x, int *y, int *buttons);

#endif /* DRAGOON_VIRTIO_INPUT_H */
