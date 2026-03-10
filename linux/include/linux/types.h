#ifndef _LINUX_TYPES_H
#define _LINUX_TYPES_H

typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef signed char         s8;
typedef signed short        s16;
typedef signed int          s32;
typedef signed long long    s64;
typedef u64                 size_t;
typedef s64                 ssize_t;
typedef u64                 uintptr_t;
typedef u64                 phys_addr_t;
typedef u64                 resource_size_t;
typedef u64                 dma_addr_t;
typedef s64                 loff_t;
#ifndef __cplusplus
#if __STDC_VERSION__ < 202311L && !defined(__bool_true_false_are_defined)
typedef _Bool               bool;
#define true  1
#define false 0
#define __bool_true_false_are_defined 1
#endif
#endif
#define NULL  ((void *)0)

#define __iomem
#define __user
#define __kernel
#define __force

typedef u32 gfp_t;
#define GFP_KERNEL 0
#define GFP_ATOMIC 1

/* Error codes */
#define EPERM    1
#define ENOENT   2
#define EIO      5
#define ENOMEM  12
#define EBUSY   16
#define ENODEV  19
#define EINVAL  22
#define ENOSYS  38

#define IS_ERR_VALUE(x) ((unsigned long)(void *)(x) >= (unsigned long)-4095)
#define IS_ERR(ptr)     IS_ERR_VALUE((unsigned long)(ptr))
#define PTR_ERR(ptr)    ((long)(ptr))
#define ERR_PTR(err)    ((void *)((long)(err)))

#endif /* _LINUX_TYPES_H */
