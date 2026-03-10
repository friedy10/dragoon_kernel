/*
 * Dragoon Microkernel - ARM Generic Timer
 *
 * Uses the EL1 virtual timer (CNTV), which fires IRQ 27 (PPI).
 * 10ms tick interval for scheduling.
 * Each CPU has its own virtual timer (PPI is per-CPU).
 */
#include "timer.h"
#include "irq.h"
#include "printf.h"
#include "types.h"
#include "waitqueue.h"
#include "percpu.h"

/* Virtual timer IRQ (PPI 27, but GIC PPI numbering = 16 + offset) */
#define TIMER_IRQ 27

static u64 timer_freq;
static u64 timer_interval;
static volatile u64 tick_count;

static void timer_handler(u32 irq)
{
    (void)irq;

    /* Only CPU 0 increments the global tick counter and checks timeouts */
    if (cpu_id() == 0) {
        tick_count++;
        wq_tick();
    }

    /* Set per-CPU reschedule flag */
    this_cpu()->reschedule_needed = 1;

    /* Rearm this CPU's timer */
    write_sysreg(cntv_tval_el0, timer_interval);
}

void timer_init(void)
{
    /* Read timer frequency */
    timer_freq = read_sysreg(cntfrq_el0);

    /* 10ms interval */
    timer_interval = timer_freq / 100;

    kprintf("[timer] frequency: %llu Hz, interval: %llu ticks (10 ms)\n",
            timer_freq, timer_interval);

    /* Register IRQ handler */
    irq_register(TIMER_IRQ, timer_handler);
    irq_enable(TIMER_IRQ);

    /* Set timer value and enable */
    write_sysreg(cntv_tval_el0, timer_interval);
    write_sysreg(cntv_ctl_el0, 1);  /* Enable, unmask */

    tick_count = 0;

    kprintf("[timer] armed, IRQ %d\n", TIMER_IRQ);
}

void timer_init_secondary(void)
{
    /* Each core has its own CNTV timer (PPI 27).
     * Handler already registered by primary. Just arm and enable. */
    u64 freq = read_sysreg(cntfrq_el0);
    u64 interval = freq / 100;

    irq_enable(TIMER_IRQ);
    write_sysreg(cntv_tval_el0, interval);
    write_sysreg(cntv_ctl_el0, 1);
}

u64 timer_get_ticks(void)
{
    return tick_count;
}
