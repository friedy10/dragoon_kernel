#ifndef _LINUX_INIT_H
#define _LINUX_INIT_H

#define __init __attribute__((section(".text")))
#define __exit __attribute__((section(".text")))
#define __initdata
#define __exitdata

#endif /* _LINUX_INIT_H */
