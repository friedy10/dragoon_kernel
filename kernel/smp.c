/*
 * Dragoon Microkernel - Symmetric Multiprocessing
 *
 * Boots secondary CPUs via PSCI CPU_ON (HVC interface).
 * Provides IPI (Inter-Processor Interrupt) for cross-CPU wakeup.
 */
#include "smp.h"
#include "mm.h"
#include "irq.h"
#include "timer.h"
#include "sched.h"
#include "printf.h"

/* PSCI function IDs (SMC Calling Convention, 64-bit) */
#define PSCI_CPU_ON     0xC4000003ULL

/* GIC SGI register for IPI */
#define GICD_SGIR       (GICD_BASE + 0xF00)
#define IPI_RESCHEDULE  0

/* Per-CPU stack pointers, indexed by CPU ID */
u64 secondary_stacks[MAX_CPUS];

/* Number of CPUs currently online */
volatile int num_cpus_online = 1;

/* Secondary CPU entry point (assembly, in boot.S) */
extern void _secondary_start(void);

static int psci_cpu_on(u64 target_cpu, u64 entry_point, u64 context_id)
{
    register u64 x0 __asm__("x0") = PSCI_CPU_ON;
    register u64 x1 __asm__("x1") = target_cpu;
    register u64 x2 __asm__("x2") = entry_point;
    register u64 x3 __asm__("x3") = context_id;

    __asm__ volatile(
        "hvc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x3)
        : "memory"
    );

    return (int)(s64)x0;  /* 0 = success, negative = error */
}

/* C entry point for secondary CPUs (called from _secondary_start) */
void secondary_main(void)
{
    int id = cpu_id();

    /* Initialize per-CPU data */
    percpu[id].cpu_id = id;
    percpu[id].current_task_id = -1;
    percpu[id].reschedule_needed = 0;

    /* Initialize GIC CPU interface for this core */
    gic_cpu_init();

    /* Initialize timer for this core */
    timer_init_secondary();

    /* Enable IPI (SGI 0) on this CPU */
    irq_enable(IPI_RESCHEDULE);

    /* Enable IRQs */
    __asm__ volatile("msr daifclr, #2");

    /* Announce readiness */
    num_cpus_online++;
    kprintf("[smp] CPU %d online\n", id);

    /* Enter scheduler loop — does not return */
    sched_start();
}

void smp_init(int num_cpus)
{
    if (num_cpus <= 1 || num_cpus > MAX_CPUS)
        num_cpus = 1;

    kprintf("[smp] booting %d secondary CPUs\n", num_cpus - 1);

    for (int cpu = 1; cpu < num_cpus; cpu++) {
        /* Allocate 64KB stack for this CPU (16 pages) */
        void *stack = pages_alloc(16);
        if (!stack) {
            kprintf("[smp] failed to allocate stack for CPU %d\n", cpu);
            continue;
        }
        secondary_stacks[cpu] = (u64)stack + 16 * PAGE_SIZE;  /* stack top */

        kprintf("[smp] booting CPU %d, stack=%p\n", cpu, stack);

        int ret = psci_cpu_on((u64)cpu, (u64)_secondary_start, (u64)cpu);
        if (ret != 0) {
            kprintf("[smp] PSCI CPU_ON failed for CPU %d: ret=%d\n", cpu, ret);
            pages_free(stack, 16);
        }
    }

    /* Wait briefly for secondaries to come online */
    for (volatile int i = 0; i < 1000000; i++)
        ;

    kprintf("[smp] %d CPUs online\n", num_cpus_online);
}

void smp_send_reschedule(void)
{
    /* GICD_SGIR: target filter = 01b (all other CPUs), SGIINTID = 0 */
    u32 sgir = (1U << 24) | IPI_RESCHEDULE;
    *(volatile u32 *)(u64)GICD_SGIR = sgir;
}
