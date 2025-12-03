BITS 64

extern printf ; Declare the C printf function

section .data
    message db "Hello, World!", 10, 0 ; String with newline and null terminator
    format db "%s", 0 ; Format string for printf

section .text
    global main ; Make main visible to the linker

main:
    ; Set up the stack frame (required for Windows x64 calling convention)
    push rbp
    mov rbp, rsp
    sub rsp, 32 ; Allocate shadow space for function calls

    ; Prepare arguments for printf
    mov rcx, format ; First argument (format string)
    mov rdx, message ; Second argument (string to print)

    ; Call printf
    call printf

    ; Clean up the stack
    add rsp, 32
    pop rbp

    ; Return 0 (success)
    mov rax, 0
    ret