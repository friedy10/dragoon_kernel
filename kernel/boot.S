/*
 * Dragoon Microkernel - ARM64 Boot Code
 * Entry point for QEMU virt machine (-kernel mode)
 *
 * QEMU loads us at 0x40080000 and enters at EL1 (with -kernel flag)
 * or EL2 (bare metal). We handle both cases.
 *
 * Secondary CPUs are booted via PSCI CPU_ON and enter at _secondary_start.
 */

.section .text.boot
.global _start

_start:
    /* Disable all interrupts */
    msr     daifset, #0xf

    /* Read current exception level */
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    and     x0, x0, #3

    cmp     x0, #2
    b.eq    drop_to_el1
    cmp     x0, #1
    b.eq    at_el1

    /* EL3 - should not happen on QEMU virt with -kernel */
    b       halt

drop_to_el1:
    /* Configure EL2 for EL1 entry */

    /* HCR_EL2: RW=1 (AArch64 at EL1) */
    mov     x0, #(1 << 31)
    msr     hcr_el2, x0

    /* SPSR_EL2: EL1h (EL1 with SP_EL1), all interrupts masked */
    mov     x0, #0x3c5
    msr     spsr_el2, x0

    /* ELR_EL2: return address = at_el1 */
    adr     x0, at_el1
    msr     elr_el2, x0

    /* SCTLR_EL1: reset value, MMU off */
    mov     x0, #0
    msr     sctlr_el1, x0

    eret

at_el1:
    /* We are now at EL1 */

    /* Only boot on CPU 0, park others in holding pen */
    mrs     x0, mpidr_el1
    and     x0, x0, #0xff
    cbnz    x0, halt

    /* Zero BSS */
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
bss_loop:
    cmp     x0, x1
    b.ge    bss_done
    str     xzr, [x0], #8
    b       bss_loop
bss_done:

    /* Set up stack (grows down from _stack_top) */
    ldr     x0, =_stack_top
    mov     sp, x0

    /* Jump to C kernel main */
    bl      kernel_main

    /* Should not return, but halt if it does */
halt:
    wfe
    b       halt

/*
 * Secondary CPU entry point.
 * Called via PSCI CPU_ON. Arrives at EL1 (PSCI handles EL2->EL1).
 * x0 = context_id = CPU ID (passed by primary via PSCI)
 */
.section .text
.global _secondary_start
_secondary_start:
    /* Disable all interrupts */
    msr     daifset, #0xf

    /* x0 = CPU ID from PSCI context_id.
     * Load per-CPU stack from secondary_stacks[cpu_id]. */
    ldr     x1, =secondary_stacks
    ldr     x1, [x1, x0, lsl #3]   /* x1 = secondary_stacks[cpu_id] */
    mov     sp, x1

    /* Install vector table */
    ldr     x2, =exception_vector_table
    msr     vbar_el1, x2
    isb

    /* Initialize MMU with same config as primary */
    /* Save CPU ID across function call */
    stp     x0, x30, [sp, #-16]!
    bl      secondary_mmu_init
    ldp     x0, x30, [sp], #16

    /* Enter C code for secondary CPU init */
    bl      secondary_main

    /* Should not return */
1:  wfe
    b       1b
