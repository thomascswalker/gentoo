%include "examples/common.asm"

global main
global array_new
global array_subscript_set

section .data
    byte_count: dq 0x100                            ; 64-bit

    fmt_int: db "INT: %ld", 10, 0      ; 64-bit integer format
    fmt_malloc_result: db "Allocated %ld bytes at %p", 10, 0

    array_ptr: dq 0

section .text

extern exit
extern malloc
extern realloc
extern free
extern printf

%macro PRINT_INT 1
    mov rsi, %1                 
    lea rdi, [rel fmt_int]    
    xor eax, eax                
    xor rax, rax
    call printf
%endmacro

%macro ARRAY_NEW 3
    ; Parameters: %1 = array name, %2 = array size, %3 = elem_size
    ALIGN_STACK
    ; Load size and multiply by elem_size to get total bytes
    mov rax, QWORD [%2]
    imul rax, %3
    push rax
    call array_new
    add rsp, 16                 ; Clean up the pushed arguments from the stack
    mov [rel %1], rax    ; Move returned pointer into specified var
%endmacro

%macro SET_ARRAY_ELEM 4
    ; Parameters: %1 = array name, %2 = index, %3 = value, %4 = elem_size
    ; Compute offset = index * elem_size. For small constant elem_size, NASM will optimize.
    mov rdx, %2
    imul rdx, %4
    add rdx, %1

    %if %4 = 1
        mov BYTE [rdx], %3
    %elif %4 = 2
        mov WORD [rdx], %3
    %elif %4 = 4
        mov DWORD [rdx], %3
    %elif %4 = 8
        mov QWORD [rdx], %3
    %else
        ; Invalid element size
        ; Must be 1, 2, 4, or 8
        mov rdi, 2
        call exit
    %endif
%endmacro

%macro GET_ARRAY_ELEM 4
    ; %1 = base ptr, %2 = index, %3 = destination register (64-bit), %4 = elem_size
    mov rdx, %2
    imul rdx, %4
    add rdx, %1
    %if %4 = 1
        movzx %3, BYTE [rdx]
    %elif %4 = 2
        movzx %3, WORD [rdx]
    %elif %4 = 4
        movzx %3, DWORD [rdx]
    %elif %4 = 8
        mov %3, QWORD [rdx]
    %else
        ; Invalid element size
        ; Must be 1, 2, 4, or 8
        mov rdi, 2
        call exit
    %endif
%endmacro

%macro PRINT_ARRAY_ELEM 3
    ; %1 = base ptr, %2 = index, %3 = elem_size
    mov rbx, rax ; save rax, by calling printf we will override it

    mov rdx, %2
    imul rdx, %3
    add rdx, %1
    %if %3 = 1                  ; 1-byte (char)
        movzx rsi, BYTE [rdx]
    %elif %3 = 2                ; 2-bytes (short)
        movzx rsi, WORD [rdx]
    %elif %3 = 4                ; 4-bytes (int)
        movzx rsi, DWORD [rdx]
    %elif %3 = 8                ; 8-bytes (long)
        mov rsi, QWORD [rdx]
    %else
        ; Invalid element size
        ; Must be 1, 2, 4, or 8
        mov rdi, 2
        call exit
    %endif
    lea rdi, [rel fmt_int]
    xor eax, eax
    xor rax, rax
    call printf

    mov rax, rbx    ; restore rax
%endmacro

%define ELEM_SIZE 2

main:
    ; Create a new array: `array_ptr = malloc(byte_count)`
    ; element size = 1 (bytes)
    ARRAY_NEW array_ptr, byte_count, ELEM_SIZE

    SET_ARRAY_ELEM array_ptr, 0, 5, ELEM_SIZE    ; array[0] = 5
    SET_ARRAY_ELEM array_ptr, 1, 10, ELEM_SIZE   ; array[1] = 10
    SET_ARRAY_ELEM array_ptr, 2, 15, ELEM_SIZE   ; array[2] = 15
    SET_ARRAY_ELEM array_ptr, 3, 50, ELEM_SIZE   ; array[2] = 15

    PRINT_ARRAY_ELEM array_ptr, 0, ELEM_SIZE
    PRINT_ARRAY_ELEM array_ptr, 1, ELEM_SIZE
    PRINT_ARRAY_ELEM array_ptr, 2, ELEM_SIZE
    PRINT_ARRAY_ELEM array_ptr, 3, ELEM_SIZE

    ; Exit
    mov rdi, 0
    call exit

; array_new
;
; args:
;   [rbp + 00]: stack pointer
;   [rbp + 08]: return pointer
;   [rbp + 16]: size (pushed by caller)
array_new:
    enter

    PRINT_INT [ARG1]

    ; Call malloc(size) where size is on the caller's stack at [rbp+16]
    mov rdi, [ARG1]
    xor rax, rax
    call malloc

    ; Did malloc succeed?
    test rax, rax
    ; false: exit program
    jz .malloc_fail
    ; true: continue
    jmp .malloc_success
.malloc_fail:
    mov rdi, 1
    call exit
.malloc_success:
    ; after malloc, rax = ptr
    mov rbx, rax          ; save return value (rbx is callee-saved)

    lea rdi, [rel fmt_malloc_result]
    mov rsi, [ARG1]
    mov rdx, rbx
    xor rax, rax
    call printf

    mov rax, rbx          ; restore return value

    leave
