/*
 * Dragoon Microkernel - Virtio Input Driver
 *
 * Handles virtio-keyboard-device and virtio-tablet-device.
 * Identifies device type via virtio-input config space.
 * Translates Linux evdev keycodes to Dragoon key codes.
 * Tablet uses absolute coordinates (EV_ABS) for trackpad support.
 * Poll-based (called from input_poll).
 */
#include "virtio_input.h"
#include "virtio.h"
#include "input.h"
#include "fb.h"
#include "printf.h"

/* Virtio input config space selectors */
#define VIRTIO_INPUT_CFG_UNSET    0x00
#define VIRTIO_INPUT_CFG_ID_NAME  0x01
#define VIRTIO_INPUT_CFG_EV_BITS  0x11

/* Config space offsets (relative to device config at MMIO+0x100) */
#define VINPUT_CFG_SELECT  0
#define VINPUT_CFG_SUBSEL  1
#define VINPUT_CFG_SIZE    2
#define VINPUT_CFG_DATA    8

/* Event buffers: pre-allocated for virtio event queue */
#define EVENT_BUF_COUNT 16

static struct virtio_input_event kbd_events[EVENT_BUF_COUNT] __aligned(8);
static struct virtio_input_event tablet_events[EVENT_BUF_COUNT] __aligned(8);

static struct virtio_dev *kbd_dev;
static struct virtio_dev *tablet_dev;

/* Keyboard ring buffer */
#define KEY_RING_SIZE 32
static int key_ring[KEY_RING_SIZE];
static int key_ring_head;
static int key_ring_tail;

static void key_ring_push(int key)
{
    int next = (key_ring_head + 1) % KEY_RING_SIZE;
    if (next == key_ring_tail)
        return; /* full */
    key_ring[key_ring_head] = key;
    key_ring_head = next;
}

static int key_ring_pop(void)
{
    if (key_ring_head == key_ring_tail)
        return KEY_NONE;
    int key = key_ring[key_ring_tail];
    key_ring_tail = (key_ring_tail + 1) % KEY_RING_SIZE;
    return key;
}

/* Mouse/tablet state */
static int mouse_x;
static int mouse_y;
static int mouse_btns;

/*
 * Linux evdev keycode -> Dragoon key code mapping.
 * Table indexed by Linux keycode (0-127).
 * 0 = unmapped.
 */
static const u8 keycode_table[128] = {
    [1]  = KEY_ESC,
    [2]  = '1', [3]  = '2', [4]  = '3', [5]  = '4', [6]  = '5',
    [7]  = '6', [8]  = '7', [9]  = '8', [10] = '9', [11] = '0',
    [12] = '-', [13] = '=',
    [14] = KEY_BACKSPACE,
    [15] = '\t',
    [16] = 'q', [17] = 'w', [18] = 'e', [19] = 'r', [20] = 't',
    [21] = 'y', [22] = 'u', [23] = 'i', [24] = 'o', [25] = 'p',
    [26] = '[', [27] = ']',
    [28] = KEY_ENTER,
    [30] = 'a', [31] = 's', [32] = 'd', [33] = 'f', [34] = 'g',
    [35] = 'h', [36] = 'j', [37] = 'k', [38] = 'l',
    [39] = ';', [40] = '\'',
    [41] = '`',
    [43] = '\\',
    [44] = 'z', [45] = 'x', [46] = 'c', [47] = 'v', [48] = 'b',
    [49] = 'n', [50] = 'm',
    [51] = ',', [52] = '.', [53] = '/',
    [57] = KEY_SPACE,
    [103] = KEY_UP,
    [105] = KEY_LEFT,
    [106] = KEY_RIGHT,
    [108] = KEY_DOWN,
};

/* Shift map for uppercase / shifted symbols */
static const u8 shift_table[128] = {
    [2]  = '!', [3]  = '@', [4]  = '#', [5]  = '$', [6]  = '%',
    [7]  = '^', [8]  = '&', [9]  = '*', [10] = '(', [11] = ')',
    [12] = '_', [13] = '+',
    [16] = 'Q', [17] = 'W', [18] = 'E', [19] = 'R', [20] = 'T',
    [21] = 'Y', [22] = 'U', [23] = 'I', [24] = 'O', [25] = 'P',
    [26] = '{', [27] = '}',
    [30] = 'A', [31] = 'S', [32] = 'D', [33] = 'F', [34] = 'G',
    [35] = 'H', [36] = 'J', [37] = 'K', [38] = 'L',
    [39] = ':', [40] = '"',
    [41] = '~',
    [43] = '|',
    [44] = 'Z', [45] = 'X', [46] = 'C', [47] = 'V', [48] = 'B',
    [49] = 'N', [50] = 'M',
    [51] = '<', [52] = '>', [53] = '?',
};

static int shift_held;

static void process_kbd_event(struct virtio_input_event *ev)
{
    if (ev->type == EV_SYN)
        return;

    if (ev->type == EV_KEY) {
        u16 code = ev->code;
        u32 value = ev->value; /* 1=press, 0=release, 2=repeat */

        /* Track shift state */
        if (code == 42 || code == 54) {
            shift_held = (value != 0) ? 1 : 0;
            return;
        }

        /* Only handle press and repeat */
        if (value == 0)
            return;

        if (code < 128) {
            int key;
            if (shift_held && shift_table[code])
                key = shift_table[code];
            else
                key = keycode_table[code];
            if (key)
                key_ring_push(key);
        }
    }
}

static void process_tablet_event(struct virtio_input_event *ev)
{
    if (ev->type == EV_SYN)
        return;

    /* Absolute positioning (virtio-tablet-device / trackpad) */
    if (ev->type == EV_ABS) {
        s32 val = (s32)ev->value;
        if (ev->code == ABS_X) {
            if (val < 0) val = 0;
            if (val > TABLET_ABS_MAX) val = TABLET_ABS_MAX;
            mouse_x = (int)(((s64)val * (FB_WIDTH - 1)) / TABLET_ABS_MAX);
        } else if (ev->code == ABS_Y) {
            if (val < 0) val = 0;
            if (val > TABLET_ABS_MAX) val = TABLET_ABS_MAX;
            mouse_y = (int)(((s64)val * (FB_HEIGHT - 1)) / TABLET_ABS_MAX);
        }
    }

    /* Relative mouse movement fallback */
    if (ev->type == EV_REL) {
        if (ev->code == REL_X) {
            mouse_x += (s32)ev->value;
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_x >= FB_WIDTH) mouse_x = FB_WIDTH - 1;
        } else if (ev->code == REL_Y) {
            mouse_y += (s32)ev->value;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_y >= FB_HEIGHT) mouse_y = FB_HEIGHT - 1;
        }
    }

    /* Button events */
    if (ev->type == EV_KEY) {
        int bit = 0;
        if (ev->code == BTN_LEFT)   bit = MOUSE_BTN_LEFT;
        if (ev->code == BTN_RIGHT)  bit = MOUSE_BTN_RIGHT;
        if (bit) {
            if (ev->value)
                mouse_btns |= bit;
            else
                mouse_btns &= ~bit;
        }
    }
}

static void refill_event_queue(struct virtio_dev *dev,
                               struct virtio_input_event *bufs)
{
    struct virtq *vq = &dev->queues[0];
    for (int i = 0; i < EVENT_BUF_COUNT; i++) {
        virtq_push_buf(vq, &bufs[i],
                        (u32)sizeof(struct virtio_input_event),
                        VIRTQ_DESC_F_WRITE);
    }
    virtq_kick(vq);
}

static void drain_events(struct virtio_dev *dev,
                          struct virtio_input_event *bufs,
                          void (*handler)(struct virtio_input_event *))
{
    if (!dev || !dev->active)
        return;

    struct virtq *vq = &dev->queues[0];
    int refill_count = 0;

    while (virtq_has_used(vq)) {
        u32 len;
        int desc_id = virtq_pop_used(vq, &len);
        if (desc_id < 0)
            break;

        if (len >= sizeof(struct virtio_input_event))
            handler(&bufs[desc_id]);

        /* Re-submit buffer */
        virtq_push_buf(vq, &bufs[desc_id],
                        (u32)sizeof(struct virtio_input_event),
                        VIRTQ_DESC_F_WRITE);
        refill_count++;
    }

    if (refill_count > 0)
        virtq_kick(vq);
}

/*
 * Query virtio-input config to get device name.
 * Returns length of name read, or 0 on failure.
 */
static int query_input_name(struct virtio_dev *dev, char *out, int max_len)
{
    /* Write select=ID_NAME, subsel=0 */
    virtio_config_write8(dev, VINPUT_CFG_SELECT, VIRTIO_INPUT_CFG_ID_NAME);
    virtio_config_write8(dev, VINPUT_CFG_SUBSEL, 0);
    dsb();

    u8 size = virtio_config_read8(dev, VINPUT_CFG_SIZE);
    if (size == 0 || size > 128)
        return 0;

    int n = (int)size < max_len - 1 ? (int)size : max_len - 1;
    for (int i = 0; i < n; i++)
        out[i] = (char)virtio_config_read8(dev, VINPUT_CFG_DATA + (u32)i);
    out[n] = 0;

    /* Reset select */
    virtio_config_write8(dev, VINPUT_CFG_SELECT, VIRTIO_INPUT_CFG_UNSET);

    return n;
}

/* Check if string contains a substring (case-insensitive for first letter) */
static int str_contains(const char *haystack, const char *needle)
{
    for (int i = 0; haystack[i]; i++) {
        int match = 1;
        for (int j = 0; needle[j]; j++) {
            char h = haystack[i + j];
            char n = needle[j];
            if (h == 0) { match = 0; break; }
            /* Simple case-insensitive: lowercase both */
            if (h >= 'A' && h <= 'Z') h += 32;
            if (n >= 'A' && n <= 'Z') n += 32;
            if (h != n) { match = 0; break; }
        }
        if (match) return 1;
    }
    return 0;
}

int virtio_input_init(void)
{
    key_ring_head = 0;
    key_ring_tail = 0;
    mouse_x = FB_WIDTH / 2;
    mouse_y = FB_HEIGHT / 2;
    mouse_btns = 0;
    shift_held = 0;
    kbd_dev = NULL;
    tablet_dev = NULL;

    /*
     * Identify input devices by querying their config space name.
     * Both keyboard and tablet have DeviceID=18 (virtio-input).
     * QEMU assigns slots in reverse order, so we can't rely on index.
     */
    for (int idx = 0; ; idx++) {
        struct virtio_dev *dev = virtio_find_dev(VIRTIO_DEV_INPUT, idx);
        if (!dev)
            break;

        /* We need ACKNOWLEDGE+DRIVER to read config on some QEMU versions */
        u64 base = dev->base;
        /* Temporarily set status to read config */
        *(volatile u32 *)(base + VIRTIO_MMIO_STATUS) = 0; /* reset */
        dsb();
        *(volatile u32 *)(base + VIRTIO_MMIO_STATUS) = VIRTIO_STATUS_ACKNOWLEDGE;
        *(volatile u32 *)(base + VIRTIO_MMIO_STATUS) =
            VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
        dsb();

        char name[64];
        int len = query_input_name(dev, name, sizeof(name));

        /* Reset device so virtio_dev_init can do full negotiation */
        *(volatile u32 *)(base + VIRTIO_MMIO_STATUS) = 0;
        dsb();

        if (len > 0) {
            kprintf("[vinput] dev at 0x%llx: \"%s\"\n", base, name);
        } else {
            kprintf("[vinput] dev at 0x%llx: (no name, len=%d)\n", base, len);
        }

        if (str_contains(name, "keyboard") || str_contains(name, "Keyboard")) {
            kbd_dev = dev;
        } else {
            /* Anything else (tablet, mouse, touchpad) → pointer device */
            tablet_dev = dev;
        }
    }

    /* Initialize keyboard */
    if (kbd_dev) {
        if (virtio_dev_init(kbd_dev, 1) == 0) {
            kprintf("[vinput] keyboard ready\n");
            refill_event_queue(kbd_dev, kbd_events);
        } else {
            kprintf("[vinput] keyboard init FAILED\n");
            kbd_dev = NULL;
        }
    }

    /* Initialize tablet/mouse */
    if (tablet_dev) {
        if (virtio_dev_init(tablet_dev, 1) == 0) {
            kprintf("[vinput] tablet/mouse ready\n");
            refill_event_queue(tablet_dev, tablet_events);
        } else {
            kprintf("[vinput] tablet init FAILED\n");
            tablet_dev = NULL;
        }
    }

    if (!kbd_dev && !tablet_dev) {
        kprintf("[vinput] no input devices found!\n");
        return -1;
    }

    return 0;
}

int virtio_kbd_poll(void)
{
    drain_events(kbd_dev, kbd_events, process_kbd_event);
    return key_ring_pop();
}

void virtio_mouse_poll(int *x, int *y, int *buttons)
{
    drain_events(tablet_dev, tablet_events, process_tablet_event);
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
    if (buttons) *buttons = mouse_btns;
}
