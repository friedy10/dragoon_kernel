/*
 * Dragoon Microkernel - GICv2 Interrupt Controller
 *
 * QEMU virt machine uses GICv2 at:
 *   Distributor: 0x08000000
 *   CPU interface: 0x08010000
 */
#include "irq.h"
#include "percpu.h"
#include "printf.h"
#include "types.h"

/* GIC Distributor registers */
#define GICD_CTLR       (GICD_BASE + 0x000)
#define GICD_TYPER      (GICD_BASE + 0x004)
#define GICD_ISENABLER  (GICD_BASE + 0x100)
#define GICD_ICENABLER  (GICD_BASE + 0x180)
#define GICD_ISPENDR    (GICD_BASE + 0x200)
#define GICD_ICPENDR    (GICD_BASE + 0x280)
#define GICD_IPRIORITYR (GICD_BASE + 0x400)
#define GICD_ITARGETSR  (GICD_BASE + 0x800)
#define GICD_ICFGR      (GICD_BASE + 0xC00)

/* GIC CPU interface registers */
#define GICC_CTLR       (GICC_BASE + 0x000)
#define GICC_PMR        (GICC_BASE + 0x004)
#define GICC_IAR        (GICC_BASE + 0x00C)
#define GICC_EOIR       (GICC_BASE + 0x010)

static inline void gic_write(u64 addr, u32 val)
{
    *(volatile u32 *)addr = val;
}

static inline u32 gic_read(u64 addr)
{
    return *(volatile u32 *)addr;
}

#define MAX_IRQ 256
#define IPI_RESCHEDULE 0  /* SGI 0 = reschedule IPI */
#define GICD_SGIR (GICD_BASE + 0xF00)

static irq_handler_t irq_handlers[MAX_IRQ];

/* Default handler for unhandled exceptions */
void exception_handler(void *regs, u64 esr, u64 elr, u64 far)
{
    u32 ec = (esr >> 26) & 0x3F;
    kprintf("\n[EXCEPTION] EC=0x%x ESR=0x%llx ELR=0x%llx FAR=0x%llx\n",
            ec, esr, elr, far);
    kprintf("[EXCEPTION] halting\n");
    while (1)
        __asm__ volatile("wfe");
}

/* Initialize GIC CPU interface — called on each CPU */
void gic_cpu_init(void)
{
    gic_write(GICC_PMR, 0xFF);
    gic_write(GICC_CTLR, 1);
}

/* IPI reschedule handler — just sets per-CPU flag */
static void ipi_handler(u32 irq)
{
    (void)irq;
    this_cpu()->reschedule_needed = 1;
}

void irq_init(void)
{
    /* Clear all handlers */
    for (int i = 0; i < MAX_IRQ; i++)
        irq_handlers[i] = NULL;

    /* Disable distributor */
    gic_write(GICD_CTLR, 0);

    /* Find number of supported IRQs */
    u32 typer = gic_read(GICD_TYPER);
    u32 num_irqs = ((typer & 0x1F) + 1) * 32;
    if (num_irqs > MAX_IRQ)
        num_irqs = MAX_IRQ;

    /* Disable all IRQs */
    for (u32 i = 0; i < num_irqs / 32; i++)
        gic_write(GICD_ICENABLER + i * 4, 0xFFFFFFFF);

    /* Clear all pending */
    for (u32 i = 0; i < num_irqs / 32; i++)
        gic_write(GICD_ICPENDR + i * 4, 0xFFFFFFFF);

    /* Set all priorities to a moderate level */
    for (u32 i = 0; i < num_irqs / 4; i++)
        gic_write(GICD_IPRIORITYR + i * 4, 0xA0A0A0A0);

    /* Target all SPIs to CPU 0 */
    for (u32 i = 8; i < num_irqs / 4; i++)
        gic_write(GICD_ITARGETSR + i * 4, 0x01010101);

    /* Enable distributor */
    gic_write(GICD_CTLR, 1);

    /* CPU interface for CPU 0 */
    gic_cpu_init();

    /* Install vector table */
    extern void vectors_init(void);
    vectors_init();

    /* Register IPI reschedule handler (SGI 0) */
    irq_register(IPI_RESCHEDULE, ipi_handler);
    irq_enable(IPI_RESCHEDULE);

    /* Enable IRQs (clear DAIF.I) */
    __asm__ volatile("msr daifclr, #2");

    kprintf("[irq] GICv2 initialized, %u IRQs\n", num_irqs);
}

void irq_register(u32 irq, irq_handler_t handler)
{
    if (irq < MAX_IRQ) {
        irq_handlers[irq] = handler;
        kprintf("[irq] registered handler for IRQ %u\n", irq);
    }
}

void irq_enable(u32 irq)
{
    u32 reg = irq / 32;
    u32 bit = irq % 32;
    gic_write(GICD_ISENABLER + reg * 4, 1 << bit);
}

void irq_disable(u32 irq)
{
    u32 reg = irq / 32;
    u32 bit = irq % 32;
    gic_write(GICD_ICENABLER + reg * 4, 1 << bit);
}

void irq_handle(void)
{
    u32 iar = gic_read(GICC_IAR);
    u32 irq = iar & 0x3FF;

    if (irq >= 1020) {
        /* Spurious interrupt */
        return;
    }

    if (irq < MAX_IRQ && irq_handlers[irq]) {
        irq_handlers[irq](irq);
    } else {
        kprintf("[irq] unhandled IRQ %u\n", irq);
    }

    /* End of interrupt */
    gic_write(GICC_EOIR, iar);
}
