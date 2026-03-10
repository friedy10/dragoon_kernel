/*
 * Dragoon Microkernel - Futex
 *
 * Linux-style futex: atomically check a word in memory and block if
 * it matches an expected value. Built on wait queues.
 */
#include "futex.h"
#include "waitqueue.h"
#include "printf.h"

#define FUTEX_MAX 16

struct futex_slot {
    u32 *addr;
    struct wait_queue wq;
    int active;
};

static struct futex_slot ftable[FUTEX_MAX];

void futex_init(void)
{
    for (int i = 0; i < FUTEX_MAX; i++) {
        ftable[i].addr = (u32 *)0;
        ftable[i].active = 0;
        wq_init(&ftable[i].wq);
    }
    kprintf("[futex] futex subsystem initialized (%d slots)\n", FUTEX_MAX);
}

/* Find or create a slot for the given address */
static int find_slot(u32 *addr, int create)
{
    int free_slot = -1;
    for (int i = 0; i < FUTEX_MAX; i++) {
        if (ftable[i].active && ftable[i].addr == addr)
            return i;
        if (!ftable[i].active && free_slot < 0)
            free_slot = i;
    }
    if (create && free_slot >= 0) {
        ftable[free_slot].addr = addr;
        ftable[free_slot].active = 1;
        wq_init(&ftable[free_slot].wq);
        return free_slot;
    }
    return -1;
}

/* Check if slot's wait queue is empty and deactivate if so */
static void maybe_free_slot(int slot)
{
    if (ftable[slot].wq.head < 0)
        ftable[slot].active = 0;
}

int futex_wait(u32 *addr, u32 expected)
{
    /* Check value — if mismatch, don't block */
    if (*addr != expected)
        return -1;

    int slot = find_slot(addr, 1);
    if (slot < 0) {
        kprintf("[futex] no free slots!\n");
        return -1;
    }

    /* Re-check after finding slot (still single-core cooperative, no race) */
    if (*addr != expected) {
        maybe_free_slot(slot);
        return -1;
    }

    wq_wait(&ftable[slot].wq);
    maybe_free_slot(slot);
    return 0;
}

int futex_wait_timeout(u32 *addr, u32 expected, u64 ticks)
{
    if (*addr != expected)
        return -1;

    int slot = find_slot(addr, 1);
    if (slot < 0) {
        kprintf("[futex] no free slots!\n");
        return -1;
    }

    if (*addr != expected) {
        maybe_free_slot(slot);
        return -1;
    }

    int ret = wq_wait_timeout(&ftable[slot].wq, ticks);
    maybe_free_slot(slot);
    return ret;
}

int futex_wake(u32 *addr, int count)
{
    int slot = find_slot(addr, 0);
    if (slot < 0)
        return 0;

    int woken = 0;
    while (woken < count && ftable[slot].wq.head >= 0) {
        wq_wake_one(&ftable[slot].wq);
        woken++;
    }

    maybe_free_slot(slot);
    return woken;
}
