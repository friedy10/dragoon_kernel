/*
 * Dragoon Microkernel - ARM Generic Timer
 *
 * Uses the EL1 virtual timer (CNTV), which fires IRQ 27 (PPI).
 * 10ms tick interval for scheduling.
 */
#include "timer.h"
#include "irq.h"
#include "printf.h"
#include "types.h"

/* Virtual timer IRQ (PPI 27, but GIC PPI numbering = 16 + offset) */
#define TIMER_IRQ 27

static u64 timer_freq;
static u64 timer_interval;
static volatile u64 tick_count;

/* Flag for cooperative reschedule */
volatile int reschedule_needed;

static void timer_handler(u32 irq)
{
    (void)irq;
    tick_count++;
    reschedule_needed = 1;

    /* Rearm timer */
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
    reschedule_needed = 0;

    kprintf("[timer] armed, IRQ %d\n", TIMER_IRQ);
}

u64 timer_get_ticks(void)
{
    return tick_count;
}
