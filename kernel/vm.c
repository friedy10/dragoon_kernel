/*
 * Dragoon Microkernel - Virtual Memory
 *
 * Per-task address space management with ASID allocation.
 * Kernel identity mapping is shared across all address spaces
 * by pointing pgd[0] at the shared L1 table.
 */
#include "vm.h"
#include "mm.h"
#include "spinlock.h"
#include "printf.h"

/* ASID bitmap: ASID 0 reserved for kernel */
static u8 asid_bitmap[ASID_MAX / 8];
static struct spinlock asid_lock = SPINLOCK_INIT;

/* Kernel's L0 table — defined in mm.c */
extern u64 pgd_table[];

static u16 asid_alloc(void)
{
    spin_lock(&asid_lock);
    for (int i = 1; i < ASID_MAX; i++) {
        if (!(asid_bitmap[i / 8] & (1 << (i % 8)))) {
            asid_bitmap[i / 8] |= (1 << (i % 8));
            spin_unlock(&asid_lock);
            return (u16)i;
        }
    }
    spin_unlock(&asid_lock);
    return 0;  /* fallback: ASID 0 */
}

static void asid_free(u16 asid)
{
    if (asid > 0 && asid < ASID_MAX) {
        spin_lock(&asid_lock);
        asid_bitmap[asid / 8] &= ~(1 << (asid % 8));
        spin_unlock(&asid_lock);
    }
}

/* Recursively free page table subtree (for levels 1-3).
 * level: 1=L1, 2=L2, 3=L3. L3 entries are leaf pages (don't free those). */
static void vm_free_subtree(u64 *table, int level)
{
    for (int i = 0; i < 512; i++) {
        if (!(table[i] & (1ULL << 0)))  /* PTE_VALID */
            continue;

        if (level < 3 && (table[i] & (1ULL << 1))) {
            /* Table descriptor — recurse into next level */
            u64 *next = (u64 *)(table[i] & ~0xFFFULL);
            vm_free_subtree(next, level + 1);
        }
        /* Block descriptors and L3 page entries: we don't free the
         * mapped physical pages here (they belong to the page allocator) */
    }

    /* Free this table page itself */
    page_free(table);
}

int vm_create(struct address_space *as)
{
    /* Allocate a fresh L0 (PGD) table — page_alloc returns zeroed page */
    as->pgd = (u64 *)page_alloc();
    if (!as->pgd)
        return -1;

    /* Share kernel's L0 entry 0 so kernel+MMIO identity mapping is visible.
     * This means pgd[0] -> same L1 table as the kernel's pgd_table[0].
     * Kernel mappings are always in sync across all address spaces. */
    as->pgd[0] = pgd_table[0];

    /* Entries 1-511 are zero (unmapped) — per-task space */

    /* Allocate ASID */
    as->asid = asid_alloc();

    return 0;
}

void vm_destroy(struct address_space *as)
{
    if (!as->pgd)
        return;

    /* Walk L0 entries 1-511 and free any allocated page tables.
     * Entry 0 is the shared kernel mapping — DO NOT free it. */
    for (int i = 1; i < 512; i++) {
        if (as->pgd[i] & (1ULL << 0)) {
            u64 *l1 = (u64 *)(as->pgd[i] & ~0xFFFULL);
            vm_free_subtree(l1, 1);
        }
    }

    page_free(as->pgd);
    asid_free(as->asid);
    as->pgd = NULL;
    as->asid = 0;
}
