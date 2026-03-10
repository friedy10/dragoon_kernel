/*
 * Dragoon - Linux Compatibility Layer
 *
 * Implements Linux kernel API functions by mapping them to
 * Dragoon microkernel capability operations.
 */
#include "../kernel/printf.h"
#include "../kernel/types.h"
#include "../kernel/mm.h"
#include "../kernel/irq.h"

/* ---- printk ---- */

int printk(const char *fmt, ...)
{
    /* Strip log level prefix like "<6>" */
    if (fmt[0] == '<' && fmt[1] >= '0' && fmt[1] <= '7' && fmt[2] == '>')
        fmt += 3;

    va_list ap;
    va_start(ap, fmt);
    kprintf("[linux] ");
    kvprintf(fmt, ap);
    va_end(ap);
    return 0;
}

/* ---- Memory allocation ---- */

/*
 * Simple bump allocator on top of page_alloc.
 * Allocates whole pages. For small allocs, we use a slab-like approach
 * with a single page divided into chunks.
 */
static u64 slab_page = 0;
static u64 slab_offset = 0;

void *kmalloc(u64 size, u32 flags)
{
    (void)flags;
    if (size == 0)
        return NULL;

    /* Small allocation: use slab */
    if (size <= 2048) {
        size = (size + 15) & ~15ULL; /* 16-byte align */
        if (slab_page == 0 || slab_offset + size > PAGE_SIZE) {
            void *p = page_alloc();
            if (!p)
                return NULL;
            slab_page = (u64)p;
            slab_offset = 0;
        }
        void *ret = (void *)(slab_page + slab_offset);
        slab_offset += size;
        return ret;
    }

    /* Large allocation: use full pages */
    u64 pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    /* For simplicity, only support single-page large allocs */
    if (pages > 1)
        return NULL;
    return page_alloc();
}

void *kzalloc(u64 size, u32 flags)
{
    void *p = kmalloc(size, flags);
    if (p) {
        u8 *bp = (u8 *)p;
        for (u64 i = 0; i < size; i++)
            bp[i] = 0;
    }
    return p;
}

void kfree(const void *ptr)
{
    if (!ptr)
        return;
    /* Only free page-aligned allocations (full pages) */
    u64 addr = (u64)ptr;
    if ((addr & (PAGE_SIZE - 1)) == 0)
        page_free((void *)ptr);
    /* Slab allocations are not individually freed (simplified) */
}

/* ---- IO mapping ---- */

/*
 * Identity mapping: physical address == virtual address.
 * ioremap just returns the physical address as a pointer.
 */
void *ioremap(u64 offset, u64 size)
{
    (void)size;
    return (void *)offset;
}

void iounmap(volatile void *addr)
{
    (void)addr;
}

/* ---- Device-managed allocation ---- */

void *devm_kzalloc(void *dev, u64 size, u32 gfp)
{
    (void)dev;
    return kzalloc(size, gfp);
}

void *devm_ioremap(void *dev, u64 offset, u64 size)
{
    (void)dev;
    return ioremap(offset, size);
}

/* ---- Platform device/driver ---- */

/* We support up to 8 registered platform devices and drivers */
#define MAX_PLATFORM_DEVICES 8
#define MAX_PLATFORM_DRIVERS 8

struct compat_platform_device {
    const char *name;
    int id;
    void *dev;         /* points to struct device within the platform_device */
    u32 num_resources;
    void *resources;   /* struct resource * */
    void *platform_device_ptr; /* the full struct platform_device */
    int active;
};

struct compat_platform_driver {
    const char *name;
    void *probe;       /* int (*probe)(struct platform_device *) */
    void *remove;      /* int (*remove)(struct platform_device *) */
    int active;
};

static struct compat_platform_device pdevices[MAX_PLATFORM_DEVICES];
static struct compat_platform_driver pdrivers[MAX_PLATFORM_DRIVERS];
static int pdevice_count = 0;
static int pdriver_count = 0;

/*
 * Access platform_device fields by known AArch64 offsets.
 * struct platform_device {
 *   const char *name;       // offset 0, size 8
 *   int id;                 // offset 8, size 4, pad 4
 *   struct device dev;      // offset 16, size 32 (4 pointers)
 *   u32 num_resources;      // offset 48, size 4, pad 4
 *   struct resource *resource; // offset 56, size 8
 * };
 */
#define PDEV_OFF_NAME       0
#define PDEV_OFF_ID         8
#define PDEV_OFF_DEV        16
#define PDEV_OFF_NUMRES     48
#define PDEV_OFF_RESOURCE   56

static inline const char *pdev_name(void *p) { return *(const char **)((u8 *)p + PDEV_OFF_NAME); }
static inline u32 pdev_num_res(void *p) { return *(u32 *)((u8 *)p + PDEV_OFF_NUMRES); }
static inline void *pdev_resource(void *p) { return *(void **)((u8 *)p + PDEV_OFF_RESOURCE); }

/*
 * Access platform_driver fields:
 * struct platform_driver {
 *   int (*probe)(...);       // offset 0, size 8
 *   int (*remove)(...);      // offset 8, size 8
 *   void (*shutdown)(...);   // offset 16, size 8
 *   struct { const char *name; } driver; // offset 24, size 8
 * };
 */
#define PDRV_OFF_PROBE    0
#define PDRV_OFF_REMOVE   8
#define PDRV_OFF_NAME     24

static inline void *pdrv_probe(void *p) { return *(void **)((u8 *)p + PDRV_OFF_PROBE); }
static inline const char *pdrv_name(void *p) { return *(const char **)((u8 *)p + PDRV_OFF_NAME); }

/* Platform device registration (called from compat server) */
int compat_register_platform_device(void *pdev_ptr)
{
    if (pdevice_count >= MAX_PLATFORM_DEVICES)
        return -1;

    struct compat_platform_device *d = &pdevices[pdevice_count++];
    d->name = pdev_name(pdev_ptr);
    d->id = 0;
    d->num_resources = pdev_num_res(pdev_ptr);
    d->resources = pdev_resource(pdev_ptr);
    d->platform_device_ptr = pdev_ptr;
    d->active = 1;

    kprintf("[compat] registered platform device '%s'\n", d->name);
    return 0;
}

/* Compare two strings */
static int streq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

/*
 * platform_driver_register - called by module_platform_driver init function
 * Matches driver name against registered device names, calls probe on match.
 */
int platform_driver_register(void *drv_ptr)
{
    if (pdriver_count >= MAX_PLATFORM_DRIVERS)
        return -1;

    struct compat_platform_driver *d = &pdrivers[pdriver_count++];
    d->name = pdrv_name(drv_ptr);
    d->probe = pdrv_probe(drv_ptr);
    d->remove = NULL;
    d->active = 1;

    kprintf("[compat] registered platform driver '%s'\n", d->name);

    /* Match against registered platform devices */
    for (int i = 0; i < pdevice_count; i++) {
        if (pdevices[i].active && streq(pdevices[i].name, d->name)) {
            kprintf("[compat] matched device '%s' with driver '%s'\n",
                    pdevices[i].name, d->name);

            /* Call probe(pdev) */
            int (*probe_fn)(void *) = (int (*)(void *))d->probe;
            int ret = probe_fn(pdevices[i].platform_device_ptr);
            kprintf("[compat] probe returned %d\n", ret);
        }
    }

    return 0;
}

void platform_driver_unregister(void *drv_ptr)
{
    (void)drv_ptr;
}

/* platform_get_resource */
void *platform_get_resource(void *pdev_ptr, unsigned int type, unsigned int num)
{
    u32 nres = pdev_num_res(pdev_ptr);
    void *res_base = pdev_resource(pdev_ptr);

    /* struct resource { u64 start; u64 end; u64 flags; const char *name; } = 32 bytes */
    unsigned int count = 0;
    for (u32 i = 0; i < nres; i++) {
        u8 *res = (u8 *)res_base + i * 32;
        u64 flags = *(u64 *)(res + 16);
        if ((flags & type) == type) {
            if (count == num)
                return res;
            count++;
        }
    }
    return NULL;
}

/* ---- IRQ ---- */

int request_irq(unsigned int irq_num, void *handler,
                unsigned long flags, const char *name, void *dev)
{
    (void)flags; (void)name; (void)dev;
    irq_register((u32)irq_num, (irq_handler_t)handler);
    irq_enable((u32)irq_num);
    kprintf("[compat] registered IRQ %u handler for '%s'\n", irq_num, name);
    return 0;
}

void free_irq(unsigned int irq_num, void *dev_id)
{
    (void)dev_id;
    irq_disable((u32)irq_num);
}

/* ---- Chrdev stubs ---- */

int register_chrdev(unsigned int major, const char *name, void *fops)
{
    (void)major; (void)fops;
    kprintf("[compat] registered chrdev '%s' major=%u\n", name, major);
    return 0;
}

void unregister_chrdev(unsigned int major, const char *name)
{
    (void)major; (void)name;
}
