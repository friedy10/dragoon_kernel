/*
 * Dragoon Microkernel - Ticket Spinlock
 *
 * ARMv8 ticket spinlock using exclusive load/store (LDAXR/STLXR).
 * Provides fair FIFO ordering. Header-only for inlining.
 */
#ifndef DRAGOON_SPINLOCK_H
#define DRAGOON_SPINLOCK_H

#include "types.h"

struct spinlock {
    volatile u32 owner;   /* currently serving ticket */
    volatile u32 next;    /* next ticket to hand out */
};

#define SPINLOCK_INIT { 0, 0 }

static inline void spin_init(struct spinlock *lock)
{
    lock->owner = 0;
    lock->next = 0;
}

static inline void spin_lock(struct spinlock *lock)
{
    u32 ticket;

    /* Atomically fetch-and-increment 'next' using LDAXR/STLXR */
    __asm__ volatile(
        "   prfm    pstl1keep, [%1]     \n"
        "1: ldaxr   %w0, [%1]           \n"
        "   add     w2, %w0, #1         \n"
        "   stlxr   w3, w2, [%1]        \n"
        "   cbnz    w3, 1b              \n"
        : "=&r"(ticket)
        : "r"(&lock->next)
        : "w2", "w3", "memory"
    );

    /* Spin until our ticket is served */
    while (__atomic_load_n(&lock->owner, __ATOMIC_ACQUIRE) != ticket)
        __asm__ volatile("wfe");
}

static inline void spin_unlock(struct spinlock *lock)
{
    __atomic_store_n(&lock->owner, lock->owner + 1, __ATOMIC_RELEASE);
    __asm__ volatile("sev");
}

/* Lock with IRQs disabled (for use in contexts where IRQ handlers
 * might also try to acquire the same lock) */
static inline u64 spin_lock_irqsave(struct spinlock *lock)
{
    u64 daif;
    __asm__ volatile("mrs %0, daif" : "=r"(daif));
    __asm__ volatile("msr daifset, #2");  /* mask IRQ */
    spin_lock(lock);
    return daif;
}

static inline void spin_unlock_irqrestore(struct spinlock *lock, u64 daif)
{
    spin_unlock(lock);
    __asm__ volatile("msr daif, %0" :: "r"(daif));
}

#endif /* DRAGOON_SPINLOCK_H */
