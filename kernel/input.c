/*
 * Dragoon Microkernel - Unified Input Layer
 *
 * Merges virtio keyboard/mouse input with UART serial input.
 * Virtio keyboard is checked first, then UART fallback.
 */
#include "input.h"
#include "virtio.h"
#include "virtio_input.h"
#include "printf.h"

/* PL011 UART registers */
#define UART_BASE 0x09000000ULL
#define UART_DR   (*(volatile u32 *)(UART_BASE + 0x00))
#define UART_FR   (*(volatile u32 *)(UART_BASE + 0x18))
#define FR_RXFE   (1 << 4)  /* RX FIFO empty */

/* Escape sequence state machine */
static int esc_state; /* 0=normal, 1=got ESC, 2=got ESC[ */
static int virtio_available;

static int uart_try_read(void)
{
    if (UART_FR & FR_RXFE)
        return -1;
    return UART_DR & 0xFF;
}

static int uart_poll(void)
{
    int c = uart_try_read();
    if (c < 0)
        return KEY_NONE;

    switch (esc_state) {
    case 0:
        if (c == 27) {
            esc_state = 1;
            return KEY_NONE;
        }
        if (c == '\r' || c == '\n')
            return KEY_ENTER;
        if (c == 127 || c == 8)
            return KEY_BACKSPACE;
        return c;
    case 1:
        esc_state = 0;
        if (c == '[') {
            esc_state = 2;
            return KEY_NONE;
        }
        return KEY_ESC;
    case 2:
        esc_state = 0;
        switch (c) {
        case 'A': return KEY_UP;
        case 'B': return KEY_DOWN;
        case 'C': return KEY_RIGHT;
        case 'D': return KEY_LEFT;
        default:  return KEY_NONE;
        }
    }
    esc_state = 0;
    return KEY_NONE;
}

void input_init(void)
{
    esc_state = 0;
    virtio_available = 0;

    int n = virtio_probe();
    if (n > 0) {
        if (virtio_input_init() == 0)
            virtio_available = 1;
    }

    kprintf("[input] initialized (virtio=%s)\n",
            virtio_available ? "yes" : "no");
}

int input_poll(void)
{
    /* Check virtio keyboard first */
    if (virtio_available) {
        int key = virtio_kbd_poll();
        if (key != KEY_NONE)
            return key;
    }

    /* Fallback to UART */
    return uart_poll();
}

void mouse_get_state(int *x, int *y, int *buttons)
{
    if (virtio_available) {
        virtio_mouse_poll(x, y, buttons);
    } else {
        if (x) *x = 0;
        if (y) *y = 0;
        if (buttons) *buttons = 0;
    }
}
