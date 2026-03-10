/*
 * Dragoon Microkernel - Virtual Memory
 *
 * Per-task address spaces with ASID support.
 * Each task gets its own L0 (PGD) page table.
 * Kernel identity mapping is shared via pgd[0].
 */
#ifndef DRAGOON_VM_H
#define DRAGOON_VM_H

#include "types.h"

#define ASID_MAX 256  /* 8-bit ASIDs */

struct address_space {
    u64 *pgd;    /* L0 page table, unique per task */
    u16 asid;    /* hardware ASID for TLB tagging */
};

/* Create a new address space with kernel mapping shared via pgd[0] */
int vm_create(struct address_space *as);

/* Destroy an address space, freeing per-task page tables (not kernel entries) */
void vm_destroy(struct address_space *as);

#endif /* DRAGOON_VM_H */
