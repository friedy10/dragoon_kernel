#ifndef _LINUX_SLAB_H
#define _LINUX_SLAB_H

#include <linux/types.h>

void *kmalloc(size_t size, gfp_t flags);
void *kzalloc(size_t size, gfp_t flags);
void kfree(const void *ptr);

#endif /* _LINUX_SLAB_H */
