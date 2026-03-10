#ifndef DRAGOON_PRINTF_H
#define DRAGOON_PRINTF_H

typedef __builtin_va_list va_list;
#define va_start(v, l) __builtin_va_start(v, l)
#define va_end(v)      __builtin_va_end(v)
#define va_arg(v, l)   __builtin_va_arg(v, l)

void kprintf(const char *fmt, ...);
void kvprintf(const char *fmt, va_list ap);

#endif /* DRAGOON_PRINTF_H */
