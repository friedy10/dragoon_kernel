/*
 * Dragoon Microkernel - ARM64 Boot Code
 * Entry point for QEMU virt machine (-kernel mode)
 *
 * QEMU loads us at 0x40080000 and enters at EL1 (with -kernel flag)
 * or EL2 (bare metal). We handle both cases.
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

    /* Only boot on CPU 0, park others */
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
