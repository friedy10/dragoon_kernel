#include "printf.h"
#include "uart.h"

static void print_char(char c)
{
    if (c == '\n')
        uart_putc('\r');
    uart_putc(c);
}

static void print_string(const char *s)
{
    if (!s)
        s = "(null)";
    while (*s)
        print_char(*s++);
}

static void print_unsigned(u64 val, int base, int width, char pad, int is_upper)
{
    char buf[64];
    int i = 0;
    const char *digits = is_upper ? "0123456789ABCDEF" : "0123456789abcdef";

    if (val == 0) {
        buf[i++] = '0';
    } else {
        while (val > 0) {
            buf[i++] = digits[val % base];
            val /= base;
        }
    }

    /* Pad */
    while (i < width)
        buf[i++] = pad;

    /* Print in reverse */
    while (i > 0)
        print_char(buf[--i]);
}

static void print_signed(s64 val, int width, char pad)
{
    if (val < 0) {
        print_char('-');
        if (width > 0)
            width--;
        print_unsigned((u64)(-val), 10, width, pad, 0);
    } else {
        print_unsigned((u64)val, 10, width, pad, 0);
    }
}

void kvprintf(const char *fmt, va_list ap)
{
    while (*fmt) {
        if (*fmt != '%') {
            print_char(*fmt++);
            continue;
        }
        fmt++; /* skip '%' */

        /* Parse flags */
        char pad = ' ';
        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }

        /* Parse width */
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /* Parse length modifier */
        int is_long = 0;
        int is_longlong = 0;
        if (*fmt == 'l') {
            is_long = 1;
            fmt++;
            if (*fmt == 'l') {
                is_longlong = 1;
                fmt++;
            }
        }

        /* Parse conversion */
        switch (*fmt) {
        case 'd':
        case 'i': {
            s64 val;
            if (is_longlong)
                val = va_arg(ap, s64);
            else if (is_long)
                val = va_arg(ap, long);
            else
                val = va_arg(ap, int);
            print_signed(val, width, pad);
            break;
        }
        case 'u': {
            u64 val;
            if (is_longlong)
                val = va_arg(ap, u64);
            else if (is_long)
                val = va_arg(ap, unsigned long);
            else
                val = va_arg(ap, unsigned int);
            print_unsigned(val, 10, width, pad, 0);
            break;
        }
        case 'x': {
            u64 val;
            if (is_longlong)
                val = va_arg(ap, u64);
            else if (is_long)
                val = va_arg(ap, unsigned long);
            else
                val = va_arg(ap, unsigned int);
            print_unsigned(val, 16, width, pad, 0);
            break;
        }
        case 'X': {
            u64 val;
            if (is_longlong)
                val = va_arg(ap, u64);
            else if (is_long)
                val = va_arg(ap, unsigned long);
            else
                val = va_arg(ap, unsigned int);
            print_unsigned(val, 16, width, pad, 1);
            break;
        }
        case 'p': {
            u64 val = (u64)va_arg(ap, void *);
            print_string("0x");
            print_unsigned(val, 16, 0, '0', 0);
            break;
        }
        case 's':
            print_string(va_arg(ap, const char *));
            break;
        case 'c':
            print_char((char)va_arg(ap, int));
            break;
        case '%':
            print_char('%');
            break;
        default:
            print_char('%');
            print_char(*fmt);
            break;
        }
        fmt++;
    }
}

void kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}
