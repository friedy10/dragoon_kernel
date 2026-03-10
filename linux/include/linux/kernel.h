#ifndef _LINUX_KERNEL_H
#define _LINUX_KERNEL_H

#include <linux/types.h>
#include <linux/printk.h>

#define container_of(ptr, type, member) ({                  \
    const typeof(((type *)0)->member) *__mptr = (ptr);      \
    (type *)((char *)__mptr - __builtin_offsetof(type, member)); })

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define BUG() do { *(volatile int *)0 = 0; } while (0)
#define BUG_ON(condition) do { if (condition) BUG(); } while (0)
#define WARN_ON(condition) ({                           \
    int __ret = !!(condition);                          \
    if (__ret) pr_warn("WARNING: %s:%d\n", __FILE__, __LINE__); \
    __ret; })

#endif /* _LINUX_KERNEL_H */
