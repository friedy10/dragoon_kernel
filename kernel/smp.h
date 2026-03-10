/*
 * Dragoon Microkernel - Symmetric Multiprocessing
 *
 * PSCI-based secondary CPU boot and IPI support.
 */
#ifndef DRAGOON_SMP_H
#define DRAGOON_SMP_H

#include "types.h"
#include "percpu.h"

/* Boot secondary CPUs via PSCI CPU_ON */
void smp_init(int num_cpus);

/* Send reschedule IPI to all other CPUs (SGI 0 via GIC) */
void smp_send_reschedule(void);

/* Number of CPUs currently online */
extern volatile int num_cpus_online;

#endif /* DRAGOON_SMP_H */
