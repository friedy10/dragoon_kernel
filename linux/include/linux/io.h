#ifndef _LINUX_IO_H
#define _LINUX_IO_H

#include <linux/types.h>

void __iomem *ioremap(phys_addr_t offset, size_t size);
void iounmap(volatile void __iomem *addr);

static inline u8 readb(const volatile void __iomem *addr)
{
    return *(const volatile u8 *)addr;
}

static inline u16 readw(const volatile void __iomem *addr)
{
    return *(const volatile u16 *)addr;
}

static inline u32 readl(const volatile void __iomem *addr)
{
    return *(const volatile u32 *)addr;
}

static inline void writeb(u8 val, volatile void __iomem *addr)
{
    *(volatile u8 *)addr = val;
}

static inline void writew(u16 val, volatile void __iomem *addr)
{
    *(volatile u16 *)addr = val;
}

static inline void writel(u32 val, volatile void __iomem *addr)
{
    *(volatile u32 *)addr = val;
}

#endif /* _LINUX_IO_H */
