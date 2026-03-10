/*
 * Hello World Linux-format driver for Dragoon
 * Tests the Linux compatibility layer
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>

struct hello_data {
    void __iomem *base;
    int value;
};

static int hello_probe(struct platform_device *pdev)
{
    struct hello_data *data;
    struct resource *res;

    pr_info("hello: probe called for device '%s'\n", pdev->name);

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        pr_err("hello: no memory resource found\n");
        return -ENODEV;
    }

    pr_info("hello: MMIO resource at 0x%llx - 0x%llx\n",
            (unsigned long long)res->start,
            (unsigned long long)res->end);

    data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->base = devm_ioremap(&pdev->dev, res->start,
                               res->end - res->start + 1);
    data->value = 42;

    dev_set_drvdata(&pdev->dev, data);

    pr_info("hello: driver loaded successfully! value=%d\n", data->value);
    pr_info("hello: *** Dragoon Linux compat layer is working! ***\n");

    return 0;
}

static int hello_remove(struct platform_device *pdev)
{
    pr_info("hello: remove called\n");
    return 0;
}

static struct platform_driver hello_driver = {
    .probe  = hello_probe,
    .remove = hello_remove,
    .driver = {
        .name = "hello",
    },
};

module_platform_driver(hello_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dragoon");
MODULE_DESCRIPTION("Hello world Linux compat test driver");
