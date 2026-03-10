#ifndef _LINUX_DEVICE_H
#define _LINUX_DEVICE_H

#include <linux/types.h>

struct device {
    const char *name;
    void *driver_data;
    void *platform_data;
    struct device *parent;
};

static inline void *dev_get_drvdata(const struct device *dev)
{
    return dev->driver_data;
}

static inline void dev_set_drvdata(struct device *dev, void *data)
{
    dev->driver_data = data;
}

/* devm_ managed allocation */
void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp);
void *devm_ioremap(struct device *dev, phys_addr_t offset, size_t size);

#endif /* _LINUX_DEVICE_H */
