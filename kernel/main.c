/*
 * Dragoon Microkernel - Main
 */
#include "types.h"
#include "uart.h"
#include "printf.h"
#include "mm.h"
#include "cap.h"
#include "task.h"
#include "sched.h"
#include "ipc.h"
#include "syscall.h"
#include "irq.h"
#include "timer.h"
#include "waitqueue.h"
#include "sleep.h"
#include "futex.h"
#include "gui.h"
#include "smp.h"

/* Linux compat server entry point */
extern void linux_compat_server(void);

/* Demo init task */
static void init_task(void)
{
    kprintf("[init] init task running\n");

    /* Demonstrate capability system */
    int mem_cap = cap_create_memory(RAM_BASE, 1024);
    kprintf("[init] created memory capability: id=%d\n", mem_cap);

    struct capability *cap = cap_lookup(mem_cap);
    if (cap) {
        kprintf("[init] cap type=%u, base=0x%llx, pages=%llu\n",
                cap->type, cap->mem.base, cap->mem.pages);
    }

    /* Demonstrate page allocation */
    void *p = page_alloc();
    kprintf("[init] allocated page at %p\n", p);
    page_free(p);

    /* Demonstrate IPC */
    int ep = ipc_endpoint_create();
    kprintf("[init] created IPC endpoint: %d\n", ep);

    kprintf("[init] init task done\n");

    /* Keep yielding */
    for (int i = 0; i < 3; i++) {
        sched_yield();
    }
}

void kernel_main(void)
{
    uart_init();

    kprintf("\n");
    kprintf("========================================\n");
    kprintf("  Dragoon Microkernel v0.1\n");
    kprintf("  Capability-based ARM64 Microkernel\n");
    kprintf("========================================\n");
    kprintf("\n");

    /* Memory management */
    mm_init();
    mm_create_page_tables();
    mm_enable_mmu();

    /* Interrupts and timer */
    irq_init();
    timer_init();

    /* Capability system */
    cap_init();

    /* Task and scheduling */
    task_init();
    sched_init();

    /* IPC and syscalls */
    ipc_init();
    syscall_init();

    /* Wait queues, sleep, futex */
    wq_pool_init();
    sleep_init();
    futex_init();

    /* Create the Linux compatibility server task */
    task_create("linux-compat", linux_compat_server);

    /* Create the init/demo task */
    task_create("init", init_task);

    /* Create the GUI task */
    task_create("gui", gui_main);

    kprintf("\n[main] kernel initialization complete\n");

    /* Boot secondary CPUs */
    smp_init(MAX_CPUS);

    kprintf("[main] starting scheduler...\n\n");

    /* Start scheduler - this does not return */
    sched_start();
}
