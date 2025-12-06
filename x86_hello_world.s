.section .data
        msg:    .asciz  "hello world\n"
        len1 = . - msg
        bye:    .asciz  "bye!\n"
        len2 = . - bye

.section .text
        .globl _start

_start:
        /* write(1, msg, len) */
        movq $1, %rdi
        leaq msg(%rip), %rsi
        movq $len1, %rdx
        movq $1, %rax
        syscall

        /* write(1, msg2, len2) */
        movq $1, %rdi
        leaq bye(%rip), %rsi
        movq $len2, %rdx
        movq $1, %rax
        syscall

        /* exit(42) */
        movq $42, %rdi
        movq $60, %rax
        syscall
