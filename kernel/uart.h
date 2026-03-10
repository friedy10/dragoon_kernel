#ifndef DRAGOON_UART_H
#define DRAGOON_UART_H

#include "types.h"

/* PL011 UART on QEMU virt machine */
#define UART_BASE 0x09000000

void uart_init(void);
void uart_putc(char c);
char uart_getc(void);
void uart_puts(const char *s);

#endif /* DRAGOON_UART_H */
