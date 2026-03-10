#ifndef DRAGOON_MM_H
#define DRAGOON_MM_H

#include "types.h"

#define PAGE_SIZE  4096
#define PAGE_SHIFT 12

/* RAM layout on QEMU virt */
#define RAM_BASE   0x40000000ULL
#define RAM_SIZE   (128 * 1024 * 1024)  /* 128 MB */

/* MMIO region */
#define MMIO_BASE  0x08000000ULL
#define MMIO_SIZE  0x02000000ULL  /* 32 MB */

void mm_init(void);
void *page_alloc(void);
void *pages_alloc(u64 n);
void page_free(void *addr);
void pages_free(void *addr, u64 n);
u64 mm_get_free_pages(void);
u64 mm_get_total_pages(void);

/* Page table management */
void mm_create_page_tables(void);
void mm_enable_mmu(void);
void mm_map_page(u64 vaddr, u64 paddr, u64 flags);

#endif /* DRAGOON_MM_H */
