#ifndef _LINUX_PLATFORM_DEVICE_H
#define _LINUX_PLATFORM_DEVICE_H

#include <linux/device.h>

/* Resource types */
#define IORESOURCE_MEM 0x00000200
#define IORESOURCE_IRQ 0x00000400

struct resource {
    resource_size_t start;   /* offset 0, 8 bytes */
    resource_size_t end;     /* offset 8, 8 bytes */
    unsigned long   flags;   /* offset 16, 8 bytes */
    const char      *name;   /* offset 24, 8 bytes */
};
/* Total: 32 bytes */

struct platform_device {
    const char      *name;
    int             id;
    struct device   dev;
    u32             num_resources;
    struct resource *resource;
};

struct platform_driver {
    int  (*probe)(struct platform_device *);
    int  (*remove)(struct platform_device *);
    void (*shutdown)(struct platform_device *);
    struct {
        const char *name;
    } driver;
};

int platform_driver_register(struct platform_driver *drv);
void platform_driver_unregister(struct platform_driver *drv);

struct resource *platform_get_resource(struct platform_device *pdev,
                                       unsigned int type, unsigned int num);

static inline void *platform_get_drvdata(const struct platform_device *pdev)
{
    return dev_get_drvdata(&((struct platform_device *)pdev)->dev);
}

static inline void platform_set_drvdata(struct platform_device *pdev, void *data)
{
    dev_set_drvdata(&pdev->dev, data);
}

#endif /* _LINUX_PLATFORM_DEVICE_H */
