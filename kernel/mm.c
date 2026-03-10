/*
 * Dragoon Microkernel - Memory Management
 *
 * Bitmap page allocator for 4KB pages.
 * ARM64 4-level page tables with 4KB granule.
 * Identity maps kernel text/data and device MMIO.
 */
#include "mm.h"
#include "spinlock.h"
#include "printf.h"

/* ---- Page Allocator ---- */

/* Bitmap: 1 bit per page. 128MB = 32768 pages = 4096 bytes bitmap */
#define MAX_PAGES (RAM_SIZE / PAGE_SIZE)
#define BITMAP_SIZE ((MAX_PAGES + 63) / 64)

static u64 page_bitmap[BITMAP_SIZE];
static u64 total_pages;
static u64 used_pages;
static struct spinlock mm_lock = SPINLOCK_INIT;

/* Kernel end symbol from linker script */
extern char __kernel_end[];
extern char _stack_top[];

static void bitmap_set(u64 idx)
{
    page_bitmap[idx / 64] |= (1ULL << (idx % 64));
}

static void bitmap_clear(u64 idx)
{
    page_bitmap[idx / 64] &= ~(1ULL << (idx % 64));
}

static int bitmap_test(u64 idx)
{
    return (page_bitmap[idx / 64] >> (idx % 64)) & 1;
}

void mm_init(void)
{
    total_pages = RAM_SIZE / PAGE_SIZE;
    used_pages = 0;

    /* Clear bitmap - all pages free */
    for (u64 i = 0; i < BITMAP_SIZE; i++)
        page_bitmap[i] = 0;

    /* Mark pages used by kernel (from RAM_BASE to _stack_top) as allocated */
    u64 kernel_end_addr = ALIGN_UP((u64)_stack_top, PAGE_SIZE);
    u64 kernel_pages = (kernel_end_addr - RAM_BASE) / PAGE_SIZE;

    for (u64 i = 0; i < kernel_pages; i++) {
        bitmap_set(i);
        used_pages++;
    }

    kprintf("[mm] initialized: %llu total pages, %llu used, %llu free\n",
            total_pages, used_pages, total_pages - used_pages);
}

void *page_alloc(void)
{
    u64 flags = spin_lock_irqsave(&mm_lock);
    for (u64 i = 0; i < MAX_PAGES; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_pages++;
            spin_unlock_irqrestore(&mm_lock, flags);
            u64 addr = RAM_BASE + i * PAGE_SIZE;
            /* Zero the page */
            u64 *p = (u64 *)addr;
            for (int j = 0; j < (int)(PAGE_SIZE / sizeof(u64)); j++)
                p[j] = 0;
            return (void *)addr;
        }
    }
    spin_unlock_irqrestore(&mm_lock, flags);
    return NULL;
}

void *pages_alloc(u64 n)
{
    if (n == 0 || n > MAX_PAGES)
        return NULL;

    u64 run_start = 0;
    u64 run_len = 0;

    u64 flags = spin_lock_irqsave(&mm_lock);
    for (u64 i = 0; i < MAX_PAGES; i++) {
        if (!bitmap_test(i)) {
            if (run_len == 0)
                run_start = i;
            run_len++;
            if (run_len == n) {
                for (u64 j = run_start; j < run_start + n; j++) {
                    bitmap_set(j);
                    used_pages++;
                }
                spin_unlock_irqrestore(&mm_lock, flags);
                u64 addr = RAM_BASE + run_start * PAGE_SIZE;
                u64 *p = (u64 *)addr;
                for (u64 j = 0; j < (n * PAGE_SIZE) / sizeof(u64); j++)
                    p[j] = 0;
                return (void *)addr;
            }
        } else {
            run_len = 0;
        }
    }
    spin_unlock_irqrestore(&mm_lock, flags);
    return NULL;
}

void page_free(void *addr)
{
    u64 a = (u64)addr;
    if (a < RAM_BASE || a >= RAM_BASE + RAM_SIZE)
        return;
    u64 idx = (a - RAM_BASE) / PAGE_SIZE;
    u64 flags = spin_lock_irqsave(&mm_lock);
    if (bitmap_test(idx)) {
        bitmap_clear(idx);
        used_pages--;
    }
    spin_unlock_irqrestore(&mm_lock, flags);
}

void pages_free(void *addr, u64 n)
{
    u64 a = (u64)addr;
    u64 flags = spin_lock_irqsave(&mm_lock);
    for (u64 i = 0; i < n; i++) {
        u64 page_addr = a + i * PAGE_SIZE;
        if (page_addr >= RAM_BASE && page_addr < RAM_BASE + RAM_SIZE) {
            u64 idx = (page_addr - RAM_BASE) / PAGE_SIZE;
            if (bitmap_test(idx)) {
                bitmap_clear(idx);
                used_pages--;
            }
        }
    }
    spin_unlock_irqrestore(&mm_lock, flags);
}

u64 mm_get_free_pages(void)
{
    return total_pages - used_pages;
}

u64 mm_get_total_pages(void)
{
    return total_pages;
}

/* ---- ARM64 Page Tables ---- */

/*
 * ARM64 4KB granule, 48-bit VA:
 *   [47:39] L0 (PGD)  - 512 entries
 *   [38:30] L1 (PUD)  - 512 entries
 *   [29:21] L2 (PMD)  - 512 entries, can use 2MB block descriptors
 *   [20:12] L3 (PTE)  - 512 entries, 4KB pages
 *
 * For simplicity, we use 1GB block descriptors at L1 for the device MMIO
 * region, and 2MB block descriptors at L2 for the RAM region.
 */

/* Page table entry bits */
#define PTE_VALID    (1ULL << 0)
#define PTE_TABLE    (1ULL << 1)  /* L0/L1/L2: points to next-level table */
#define PTE_BLOCK    (0ULL << 1)  /* L1/L2: block descriptor */
#define PTE_PAGE     (1ULL << 1)  /* L3: page descriptor */
#define PTE_AF       (1ULL << 10) /* Access flag */
#define PTE_SH_INNER (3ULL << 8)  /* Inner shareable */
#define PTE_SH_OUTER (2ULL << 8)  /* Outer shareable */

/* MAIR attribute index encoding */
#define PTE_ATTR(n)  ((u64)(n) << 2)
#define MAIR_IDX_NORMAL  0  /* Normal memory, write-back */
#define MAIR_IDX_DEVICE  1  /* Device-nGnRnE */

/* Normal memory block attributes */
#define BLOCK_NORMAL (PTE_VALID | PTE_BLOCK | PTE_AF | PTE_SH_INNER | PTE_ATTR(MAIR_IDX_NORMAL))
/* Device memory block attributes */
#define BLOCK_DEVICE (PTE_VALID | PTE_BLOCK | PTE_AF | PTE_SH_OUTER | PTE_ATTR(MAIR_IDX_DEVICE))

/* L0 table (PGD) - statically allocated, 4KB aligned.
 * Non-static: shared with vm.c for per-task address space creation. */
u64 pgd_table[512] __aligned(4096);
/* L1 table for the first 512GB */
static u64 pud_table[512] __aligned(4096);
/* L2 table for 0x00000000 - 0x3FFFFFFF (first 1GB, for device MMIO) */
static u64 pmd_table_dev[512] __aligned(4096);
/* L2 table for 0x40000000 - 0x7FFFFFFF (second 1GB, for RAM) */
static u64 pmd_table_ram[512] __aligned(4096);

void mm_create_page_tables(void)
{
    /* Clear all tables */
    for (int i = 0; i < 512; i++) {
        pgd_table[i] = 0;
        pud_table[i] = 0;
        pmd_table_dev[i] = 0;
        pmd_table_ram[i] = 0;
    }

    /* L0[0] -> L1 table */
    pgd_table[0] = ((u64)pud_table) | PTE_VALID | PTE_TABLE;

    /* L1[0] -> L2 table for 0x00000000–0x3FFFFFFF (devices) */
    pud_table[0] = ((u64)pmd_table_dev) | PTE_VALID | PTE_TABLE;

    /* L1[1] -> L2 table for 0x40000000–0x7FFFFFFF (RAM) */
    pud_table[1] = ((u64)pmd_table_ram) | PTE_VALID | PTE_TABLE;

    /* Map device MMIO region: 0x08000000 - 0x0A000000 with 2MB blocks */
    /* 0x08000000 / 2MB = index 64, 0x0A000000 / 2MB = index 80 */
    for (int i = 64; i < 82; i++) {
        u64 paddr = (u64)i * 0x200000ULL;
        pmd_table_dev[i] = paddr | BLOCK_DEVICE;
    }

    /* Map RAM: 0x40000000 - 0x48000000 (128MB) with 2MB blocks */
    /* Index 0-63 in the RAM L2 table */
    for (int i = 0; i < 64; i++) {
        u64 paddr = 0x40000000ULL + (u64)i * 0x200000ULL;
        pmd_table_ram[i] = paddr | BLOCK_NORMAL;
    }

    kprintf("[mm] page tables created at %p\n", pgd_table);
}

void mm_enable_mmu(void)
{
    /* MAIR_EL1: attribute 0 = normal WB, attribute 1 = device nGnRnE */
    u64 mair = (0xFFULL << (MAIR_IDX_NORMAL * 8)) |
               (0x00ULL << (MAIR_IDX_DEVICE * 8));
    write_sysreg(mair_el1, mair);

    /*
     * TCR_EL1:
     *   T0SZ=16 (48-bit VA)
     *   IRGN0=1 (write-back, write-allocate)
     *   ORGN0=1 (write-back, write-allocate)
     *   SH0=3   (inner shareable)
     *   TG0=0   (4KB granule)
     *   T1SZ=16
     *   TG1=2   (4KB granule for TTBR1)
     */
    u64 tcr = (16ULL << 0)  |  /* T0SZ = 16 */
              (1ULL  << 8)  |  /* IRGN0 = write-back */
              (1ULL  << 10) |  /* ORGN0 = write-back */
              (3ULL  << 12) |  /* SH0 = inner shareable */
              (0ULL  << 14) |  /* TG0 = 4KB */
              (16ULL << 16) |  /* T1SZ = 16 */
              (1ULL  << 24) |  /* IRGN1 */
              (1ULL  << 26) |  /* ORGN1 */
              (3ULL  << 28) |  /* SH1 */
              (2ULL  << 30) |  /* TG1 = 4KB */
              (1ULL  << 23);   /* EPD1 = disable TTBR1 walks */
    write_sysreg(tcr_el1, tcr);

    /* Set TTBR0_EL1 to our page table */
    write_sysreg(ttbr0_el1, (u64)pgd_table);

    /* Invalidate TLB */
    isb();
    __asm__ volatile("tlbi vmalle1is");
    dsb();
    isb();

    /* Enable MMU */
    u64 sctlr = read_sysreg(sctlr_el1);
    sctlr |= (1ULL << 0);   /* M: enable MMU */
    sctlr |= (1ULL << 2);   /* C: data cache enable */
    sctlr |= (1ULL << 12);  /* I: instruction cache enable */
    sctlr &= ~(1ULL << 19); /* WXN: disable write-implies-XN */
    write_sysreg(sctlr_el1, sctlr);

    isb();

    kprintf("[mm] MMU enabled\n");
}

void mm_map_page(u64 *pgd, u64 vaddr, u64 paddr, u64 flags)
{
    int l0_idx = (vaddr >> 39) & 0x1FF;
    int l1_idx = (vaddr >> 30) & 0x1FF;
    int l2_idx = (vaddr >> 21) & 0x1FF;
    int l3_idx = (vaddr >> 12) & 0x1FF;

    /* Walk/create L1 table */
    u64 *l1;
    if (!(pgd[l0_idx] & PTE_VALID)) {
        l1 = (u64 *)page_alloc();
        if (!l1) return;
        pgd[l0_idx] = (u64)l1 | PTE_VALID | PTE_TABLE;
    } else {
        l1 = (u64 *)(pgd[l0_idx] & ~0xFFFULL);
    }

    /* Walk/create L2 table */
    u64 *l2;
    if (!(l1[l1_idx] & PTE_VALID)) {
        l2 = (u64 *)page_alloc();
        if (!l2) return;
        l1[l1_idx] = (u64)l2 | PTE_VALID | PTE_TABLE;
    } else {
        if (!(l1[l1_idx] & PTE_TABLE)) return;  /* block descriptor, can't split */
        l2 = (u64 *)(l1[l1_idx] & ~0xFFFULL);
    }

    /* Walk/create L3 table */
    u64 *l3;
    if (!(l2[l2_idx] & PTE_VALID)) {
        l3 = (u64 *)page_alloc();
        if (!l3) return;
        l2[l2_idx] = (u64)l3 | PTE_VALID | PTE_TABLE;
    } else {
        if (!(l2[l2_idx] & PTE_TABLE)) return;  /* 2MB block, can't split */
        l3 = (u64 *)(l2[l2_idx] & ~0xFFFULL);
    }

    /* Map the 4KB page at L3 */
    l3[l3_idx] = (paddr & ~0xFFFULL) | flags;
}

void secondary_mmu_init(void)
{
    /* Set MAIR (same as primary) */
    u64 mair = (0xFFULL << (MAIR_IDX_NORMAL * 8)) |
               (0x00ULL << (MAIR_IDX_DEVICE * 8));
    write_sysreg(mair_el1, mair);

    /* Set TCR (same as primary) */
    u64 tcr = (16ULL << 0)  |  /* T0SZ = 16 */
              (1ULL  << 8)  |  /* IRGN0 = write-back */
              (1ULL  << 10) |  /* ORGN0 = write-back */
              (3ULL  << 12) |  /* SH0 = inner shareable */
              (0ULL  << 14) |  /* TG0 = 4KB */
              (16ULL << 16) |  /* T1SZ = 16 */
              (1ULL  << 24) |  /* IRGN1 */
              (1ULL  << 26) |  /* ORGN1 */
              (3ULL  << 28) |  /* SH1 */
              (2ULL  << 30) |  /* TG1 = 4KB */
              (1ULL  << 23);   /* EPD1 = disable TTBR1 walks */
    write_sysreg(tcr_el1, tcr);

    /* Set TTBR0 to kernel page tables */
    write_sysreg(ttbr0_el1, (u64)pgd_table);

    /* TLB invalidation */
    isb();
    __asm__ volatile("tlbi vmalle1is");
    dsb();
    isb();

    /* Enable MMU + caches */
    u64 sctlr = read_sysreg(sctlr_el1);
    sctlr |= (1ULL << 0);   /* M: enable MMU */
    sctlr |= (1ULL << 2);   /* C: data cache enable */
    sctlr |= (1ULL << 12);  /* I: instruction cache enable */
    sctlr &= ~(1ULL << 19); /* WXN: disable write-implies-XN */
    write_sysreg(sctlr_el1, sctlr);
    isb();
}

/* Compiler may emit calls to these for struct copies / array zeroing */
void *memcpy(void *dest, const void *src, u64 n)
{
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;
    while (n--)
        *d++ = *s++;
    return dest;
}

void *memset(void *s, int c, u64 n)
{
    u8 *p = (u8 *)s;
    while (n--)
        *p++ = (u8)c;
    return s;
}

int memcmp(const void *s1, const void *s2, u64 n)
{
    const u8 *a = (const u8 *)s1;
    const u8 *b = (const u8 *)s2;
    while (n--) {
        if (*a != *b)
            return *a - *b;
        a++;
        b++;
    }
    return 0;
}
