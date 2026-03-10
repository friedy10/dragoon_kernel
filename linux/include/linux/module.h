#ifndef _LINUX_MODULE_H
#define _LINUX_MODULE_H

#include <linux/kernel.h>

#define MODULE_LICENSE(s)
#define MODULE_AUTHOR(s)
#define MODULE_DESCRIPTION(s)
#define MODULE_ALIAS(s)
#define MODULE_DEVICE_TABLE(type, name)

#define module_init(fn) \
    static int (*__module_init)(void) __attribute__((section(".initcall.init"), used)) = fn

#define module_exit(fn) \
    static void (*__module_exit)(void) __attribute__((section(".exitcall.exit"), used)) = fn

/* module_platform_driver expands to module_init/exit that registers a platform_driver */
#define module_platform_driver(__platform_driver)                       \
    static int __init __driver_init(void)                               \
    {                                                                   \
        return platform_driver_register(&(__platform_driver));          \
    }                                                                   \
    module_init(__driver_init);                                         \
    static void __exit __driver_exit(void)                              \
    {                                                                   \
        platform_driver_unregister(&(__platform_driver));               \
    }                                                                   \
    module_exit(__driver_exit)

#define __init __attribute__((section(".text")))
#define __exit __attribute__((section(".text")))

/* Stubs for module params */
#define module_param(name, type, perm)
#define MODULE_PARM_DESC(parm, desc)

#endif /* _LINUX_MODULE_H */
