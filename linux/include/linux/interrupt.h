#ifndef _LINUX_INTERRUPT_H
#define _LINUX_INTERRUPT_H

#include <linux/types.h>

#define IRQF_TRIGGER_NONE    0x00000000
#define IRQF_TRIGGER_RISING  0x00000001
#define IRQF_TRIGGER_FALLING 0x00000002
#define IRQF_SHARED          0x00000080

typedef int irqreturn_t;
#define IRQ_NONE     0
#define IRQ_HANDLED  1

typedef irqreturn_t (*irq_handler_t)(int irq, void *dev_id);

int request_irq(unsigned int irq, irq_handler_t handler,
                unsigned long flags, const char *name, void *dev);
void free_irq(unsigned int irq, void *dev_id);

#endif /* _LINUX_INTERRUPT_H */
