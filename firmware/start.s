#include "custom_ops.S"

    .section .text
    .global _start
    .global main

reset_vec:
    // no more than 16 bytes
    j _start

.balign 16
irq_vec:
    xgsoc_retirq_insn()

irq_regs:
    // registers are saved to this memory region during interrupt handling
    // the program counter is saved as register 0
    .fill 32,4

    // stack for the interrupt handler
    .fill 128,4
irq_stack:

_start:
    add x1,x0,x0
    add x2,x0,x0
    add x3,x0,x0
    add x4,x0,x0
    add x5,x0,x0
    add x6,x0,x0
    add x7,x0,x0
    add x8,x0,x0
    add x9,x0,x0
    add x10,x0,x0
    add x11,x0,x0
    add x12,x0,x0
    add x13,x0,x0
    add x14,x0,x0
    add x15,x0,x0

    lui sp, %hi(__stacktop);
    addi sp, sp, %lo(__stacktop);

    jal ra,main
loop:
    j loop
