/*
 * Dragoon Microkernel - Sleep
 *
 * Task sleep built on wait queue timeouts.
 * sleep_ticks(N) blocks the current task for N timer ticks (10ms each).
 */
#include "sleep.h"
#include "waitqueue.h"
#include "printf.h"

static struct wait_queue sleep_wq;

void sleep_init(void)
{
    wq_init(&sleep_wq);
    kprintf("[sleep] sleep subsystem initialized\n");
}

void sleep_ticks(u64 ticks)
{
    if (ticks == 0) return;
    wq_wait_timeout(&sleep_wq, ticks);
}

void sleep_ms(u64 ms)
{
    u64 ticks = ms / 10;
    if (ticks == 0) ticks = 1;
    sleep_ticks(ticks);
}
