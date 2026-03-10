#ifndef DRAGOON_TYPES_H
#define DRAGOON_TYPES_H

typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef signed char         s8;
typedef signed short        s16;
typedef signed int          s32;
typedef signed long long    s64;

typedef u64 size_t;
typedef s64 ssize_t;
typedef u64 uintptr_t;
typedef s64 intptr_t;

/* bool is a keyword in C23+ / GCC 14+, so only define if needed */
#ifndef __cplusplus
#if __STDC_VERSION__ < 202311L && !defined(__bool_true_false_are_defined)
typedef _Bool bool;
#define true  1
#define false 0
#define __bool_true_false_are_defined 1
#endif
#endif

#define NULL ((void *)0)

#define ALIGN_UP(x, a)   (((x) + ((a) - 1)) & ~((a) - 1))
#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* Compiler attributes */
#define __packed    __attribute__((packed))
#define __aligned(n) __attribute__((aligned(n)))
#define __unused    __attribute__((unused))
#define __noreturn  __attribute__((noreturn))
#define __section(s) __attribute__((section(s)))

/* Memory barriers */
#define dmb()  __asm__ volatile("dmb sy" ::: "memory")
#define dsb()  __asm__ volatile("dsb sy" ::: "memory")
#define isb()  __asm__ volatile("isb" ::: "memory")

/* System register access */
#define read_sysreg(reg) ({                         \
    u64 _val;                                       \
    __asm__ volatile("mrs %0, " #reg : "=r"(_val)); \
    _val;                                           \
})

#define write_sysreg(reg, val) do {                         \
    u64 _val = (u64)(val);                                  \
    __asm__ volatile("msr " #reg ", %0" :: "r"(_val));      \
} while (0)

#endif /* DRAGOON_TYPES_H */
