/*
 * Dragoon Microkernel - ARM64 Exception Vectors
 *
 * Vector table: 16 entries, each 128 bytes (0x80).
 * Entries just branch to full handlers defined outside the table.
 */

.section .text

/* ---- Save/restore macros for full register state ---- */
.macro save_regs
    sub     sp, sp, #272
    stp     x0, x1, [sp, #0]
    stp     x2, x3, [sp, #16]
    stp     x4, x5, [sp, #32]
    stp     x6, x7, [sp, #48]
    stp     x8, x9, [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x19, [sp, #144]
    stp     x20, x21, [sp, #160]
    stp     x22, x23, [sp, #176]
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    stp     x28, x29, [sp, #224]
    str     x30, [sp, #240]
    mrs     x0, elr_el1
    mrs     x1, spsr_el1
    stp     x0, x1, [sp, #248]
.endm

.macro restore_regs
    ldp     x0, x1, [sp, #248]
    msr     elr_el1, x0
    msr     spsr_el1, x1
    ldp     x0, x1, [sp, #0]
    ldp     x2, x3, [sp, #16]
    ldp     x4, x5, [sp, #32]
    ldp     x6, x7, [sp, #48]
    ldp     x8, x9, [sp, #64]
    ldp     x10, x11, [sp, #80]
    ldp     x12, x13, [sp, #96]
    ldp     x14, x15, [sp, #112]
    ldp     x16, x17, [sp, #128]
    ldp     x18, x19, [sp, #144]
    ldp     x20, x21, [sp, #160]
    ldp     x22, x23, [sp, #176]
    ldp     x24, x25, [sp, #192]
    ldp     x26, x27, [sp, #208]
    ldp     x28, x29, [sp, #224]
    ldr     x30, [sp, #240]
    add     sp, sp, #272
.endm

/* ---- Vector table (each entry = branch only, fits in 128 bytes) ---- */
.balign 2048
.global exception_vector_table
exception_vector_table:
    /* From current EL, SP_EL0 (not used, we use SP_ELx) */
    .balign 128
    b       _handler_invalid
    .balign 128
    b       _handler_invalid
    .balign 128
    b       _handler_invalid
    .balign 128
    b       _handler_invalid

    /* From current EL, SP_ELx (this is what we use at EL1) */
    .balign 128
    b       _handler_sync_el1h
    .balign 128
    b       _handler_irq_el1h
    .balign 128
    b       _handler_invalid      /* FIQ */
    .balign 128
    b       _handler_invalid      /* SError */

    /* From lower EL, AArch64 (for future user-mode tasks) */
    .balign 128
    b       _handler_sync_el1h    /* treat same as current EL for now */
    .balign 128
    b       _handler_irq_el1h
    .balign 128
    b       _handler_invalid
    .balign 128
    b       _handler_invalid

    /* From lower EL, AArch32 */
    .balign 128
    b       _handler_invalid
    .balign 128
    b       _handler_invalid
    .balign 128
    b       _handler_invalid
    .balign 128
    b       _handler_invalid

/* ---- Handlers (outside vector table, no size limit) ---- */

_handler_invalid:
    save_regs
    mov     x0, sp
    mrs     x1, esr_el1
    mrs     x2, elr_el1
    mrs     x3, far_el1
    bl      exception_handler
    restore_regs
    eret

/*
 * Synchronous exception from EL1h / EL0
 * Dispatches SVC to syscall handler, others to exception_handler
 */
_handler_sync_el1h:
    save_regs

    mrs     x0, esr_el1
    lsr     x1, x0, #26         /* EC field bits [31:26] */
    cmp     x1, #0x15           /* SVC from AArch64 */
    b.eq    _do_svc

    /* Not SVC - treat as generic exception */
    mov     x0, sp
    mrs     x1, esr_el1
    mrs     x2, elr_el1
    mrs     x3, far_el1
    bl      exception_handler
    restore_regs
    eret

_do_svc:
    /* Syscall: x8=number, x0-x3=args (saved on stack) */
    ldr     x0, [sp, #64]       /* saved x8 = syscall number */
    ldr     x1, [sp, #0]        /* saved x0 = arg0 */
    ldr     x2, [sp, #8]        /* saved x1 = arg1 */
    ldr     x3, [sp, #16]       /* saved x2 = arg2 */
    ldr     x4, [sp, #24]       /* saved x3 = arg3 */
    bl      syscall_handler
    /* Return value in x0 → store back to saved x0 position */
    str     x0, [sp, #0]
    restore_regs
    eret

/*
 * IRQ from EL1h / EL0
 */
_handler_irq_el1h:
    save_regs
    bl      irq_handle
    restore_regs
    eret

/*
 * Context switch: context_switch(struct task_context *old, struct task_context *new, u64 ttbr0)
 *   x0 = pointer to old task's context
 *   x1 = pointer to new task's context
 *   x2 = new TTBR0 value (PGD | ASID<<48), or 0 to skip
 *
 * Saves callee-saved registers to old, switches TTBR0 if needed,
 * loads from new. Returns to new task's saved LR (x30).
 */
.global context_switch
context_switch:
    /* Save callee-saved regs to old context */
    stp     x19, x20, [x0, #0]
    stp     x21, x22, [x0, #16]
    stp     x23, x24, [x0, #32]
    stp     x25, x26, [x0, #48]
    stp     x27, x28, [x0, #64]
    stp     x29, x30, [x0, #80]
    mov     x9, sp
    str     x9, [x0, #96]

    /* Switch TTBR0 if x2 is non-zero */
    cbz     x2, 1f
    msr     ttbr0_el1, x2
    isb
1:

    /* Restore callee-saved regs from new context */
    ldp     x19, x20, [x1, #0]
    ldp     x21, x22, [x1, #16]
    ldp     x23, x24, [x1, #32]
    ldp     x25, x26, [x1, #48]
    ldp     x27, x28, [x1, #64]
    ldp     x29, x30, [x1, #80]
    ldr     x9, [x1, #96]
    mov     sp, x9

    ret     /* returns to new task's x30 (LR) */

/*
 * Task entry trampoline.
 * context_switch loads x19 = entry function pointer, then ret's here.
 * We call the real entry, then handle task exit.
 */
.global task_trampoline
task_trampoline:
    blr     x19             /* call the real entry function */
    bl      task_exit_handler  /* entry returned, clean up */
1:  wfe
    b       1b

/*
 * Install the vector table
 */
.global vectors_init
vectors_init:
    ldr     x0, =exception_vector_table
    msr     vbar_el1, x0
    isb
    ret
