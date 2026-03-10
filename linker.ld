/*
 * Dragoon Microkernel Linker Script
 * Target: QEMU virt machine, kernel loaded at 0x40080000
 */

ENTRY(_start)

SECTIONS
{
    . = 0x40080000;

    .text : {
        KEEP(*(.text.boot))   /* Boot code first */
        *(.text .text.*)
    }

    .rodata : ALIGN(4096) {
        *(.rodata .rodata.*)
    }

    /* Initcall section for Linux compat layer */
    .initcall.init : ALIGN(8) {
        __initcall_start = .;
        KEEP(*(.initcall.init))
        __initcall_end = .;
    }

    .data : ALIGN(4096) {
        *(.data .data.*)
    }

    .bss : ALIGN(4096) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end = .;
    }

    . = ALIGN(4096);
    __kernel_end = .;

    /* 64KB kernel stack */
    . = ALIGN(16);
    _stack_bottom = .;
    . += 0x10000;
    _stack_top = .;
}
