#ifndef DRAGOON_IRQ_H
#define DRAGOON_IRQ_H

#include "types.h"

/* GICv2 base addresses on QEMU virt */
#define GICD_BASE 0x08000000
#define GICC_BASE 0x08010000

typedef void (*irq_handler_t)(u32 irq);

void irq_init(void);
void gic_cpu_init(void);  /* per-CPU GIC interface setup */
void irq_register(u32 irq, irq_handler_t handler);
void irq_enable(u32 irq);
void irq_disable(u32 irq);
void irq_handle(void);

#endif /* DRAGOON_IRQ_H */
