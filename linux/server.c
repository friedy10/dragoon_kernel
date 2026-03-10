/*
 * Dragoon - Linux Compatibility Server
 *
 * Runs as a kernel task. Registers platform devices and
 * iterates .initcall.init section to start Linux-format drivers.
 */
#include "../kernel/printf.h"
#include "../kernel/types.h"
#include "../kernel/sched.h"
#include "compat.h"

/* Linker-provided initcall section boundaries */
extern int (*__initcall_start[])(void);
extern int (*__initcall_end[])(void);

/* Resource type flags (must match linux/platform_device.h) */
#define IORESOURCE_MEM 0x00000200

/* Platform device structures - must match linux/platform_device.h layout */
struct compat_resource {
    u64 start;
    u64 end;
    u64 flags;
    const char *name;
};

struct compat_device {
    const char *name;
    void *driver_data;
    void *platform_data;
    void *parent;
};

struct compat_platform_dev {
    const char *name;
    int id;
    struct compat_device dev;
    u32 num_resources;
    struct compat_resource *resource;
};

/* Hello device resources: MMIO region at 0x0A000000 (fake test device) */
static struct compat_resource hello_resources[] = {
    {
        .start = 0x0A000000,
        .end   = 0x0A0001FF,
        .flags = IORESOURCE_MEM,
        .name  = "hello-mmio",
    },
};

/* Hello platform device */
static struct compat_platform_dev hello_pdev = {
    .name = "hello",
    .id = 0,
    .dev = {
        .name = "hello",
        .driver_data = NULL,
        .platform_data = NULL,
        .parent = NULL,
    },
    .num_resources = 1,
    .resource = hello_resources,
};

void linux_compat_server(void)
{
    kprintf("[compat-server] Linux compatibility server starting\n");

    /* Register platform devices */
    compat_register_platform_device(&hello_pdev);

    /* Run all initcalls from .initcall.init section */
    kprintf("[compat-server] running initcalls...\n");

    int (**fn)(void) = __initcall_start;
    int count = 0;
    while (fn < __initcall_end) {
        if (*fn) {
            kprintf("[compat-server] calling initcall at %p\n", *fn);
            int ret = (*fn)();
            if (ret != 0)
                kprintf("[compat-server] initcall returned error: %d\n", ret);
            count++;
        }
        fn++;
    }

    kprintf("[compat-server] executed %d initcalls\n", count);
    kprintf("[compat-server] Linux compatibility layer initialized\n");
}
