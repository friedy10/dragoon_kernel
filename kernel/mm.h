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

/* Page table entry flags for 4KB pages at L3 */
#define PAGE_NORMAL  0x703ULL  /* Valid|Page|AF|SH_INNER|ATTR(0=normal) */
#define PAGE_DEVICE  0x607ULL  /* Valid|Page|AF|SH_OUTER|ATTR(1=device) */

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

/* Map a 4KB page in the given page table tree */
void mm_map_page(u64 *pgd, u64 vaddr, u64 paddr, u64 flags);

/* Initialize MMU on secondary CPUs (same config as primary) */
void secondary_mmu_init(void);

/* Kernel's L0 page table (shared across all address spaces via pgd[0]) */
extern u64 pgd_table[];

#endif /* DRAGOON_MM_H */
