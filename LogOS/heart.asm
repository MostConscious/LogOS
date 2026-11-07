bits 32

section .multiboot
    align 4
    dd 0x1BADB002
    dd 0x0
    dd -(0x1BADB002 + 0x0)

    section .bss
    align 16
    stack_bottom:
    resb 16384
    stack_top:

    section .text
    global _start
    extern lungs_main

    _start:
    mov esp, stack_top
    call lungs_main

    .hang:
    hlt
    jmp .hang
