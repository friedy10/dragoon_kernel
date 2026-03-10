#include "uart.h"

/* PL011 register offsets */
#define UART_DR     0x000   /* Data Register */
#define UART_FR     0x018   /* Flag Register */
#define UART_IBRD   0x024   /* Integer Baud Rate */
#define UART_FBRD   0x028   /* Fractional Baud Rate */
#define UART_LCR_H  0x02C   /* Line Control */
#define UART_CR     0x030   /* Control Register */
#define UART_IMSC   0x038   /* Interrupt Mask Set/Clear */
#define UART_ICR    0x044   /* Interrupt Clear */

/* Flag register bits */
#define FR_TXFF (1 << 5)    /* TX FIFO full */
#define FR_RXFE (1 << 4)    /* RX FIFO empty */

static inline void mmio_write(u64 base, u32 offset, u32 val)
{
    *(volatile u32 *)(base + offset) = val;
}

static inline u32 mmio_read(u64 base, u32 offset)
{
    return *(volatile u32 *)(base + offset);
}

void uart_init(void)
{
    /* Disable UART */
    mmio_write(UART_BASE, UART_CR, 0);

    /* Clear pending interrupts */
    mmio_write(UART_BASE, UART_ICR, 0x7FF);

    /* Set baud rate (115200 @ 24MHz UARTCLK on QEMU virt) */
    /* Divider = 24000000 / (16 * 115200) = 13.0208 */
    mmio_write(UART_BASE, UART_IBRD, 13);
    mmio_write(UART_BASE, UART_FBRD, 1);

    /* 8N1, enable FIFOs */
    mmio_write(UART_BASE, UART_LCR_H, (3 << 5) | (1 << 4));

    /* Mask all interrupts */
    mmio_write(UART_BASE, UART_IMSC, 0);

    /* Enable UART, TX, RX */
    mmio_write(UART_BASE, UART_CR, (1 << 0) | (1 << 8) | (1 << 9));
}

void uart_putc(char c)
{
    /* Wait for TX FIFO to have space */
    while (mmio_read(UART_BASE, UART_FR) & FR_TXFF)
        ;
    mmio_write(UART_BASE, UART_DR, (u32)c);
}

char uart_getc(void)
{
    /* Wait for RX FIFO to have data */
    while (mmio_read(UART_BASE, UART_FR) & FR_RXFE)
        ;
    return (char)(mmio_read(UART_BASE, UART_DR) & 0xFF);
}

void uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n')
            uart_putc('\r');
        uart_putc(*s++);
    }
}
