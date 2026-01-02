%include "examples/common.asm"

global main
global array_malloc
global array_free

section .data
    array_count: dq 16

    fmt_int: db "INT: %ld", 10, 0
    fmt_malloc_result: db "Allocated %ld bytes at %p", 10, 0

    array_ptr: dq 0

section .text

extern exit
extern malloc
extern realloc
extern free
extern printf

%define CHAR_ 1
%define SHORT_ 2
%define INT_ 4
%define LONG_ 8

%macro PRINT_INT 1
    mov rsi, %1                 
    lea rdi, [rel fmt_int]    
    xor eax, eax                
    xor rax, rax
    call printf
%endmacro

%macro NEW_ARRAY 3
    ; Parameters: %1 = array name, %2 = array size, %3 = elem_size
    ALIGN_STACK
    mov rax, QWORD [%2]     ; Load size
    imul rax, %3            ; multiply by elem_size to get total bytes
    push rax                ; Push total byte size as arg 1
    call array_malloc
    add rsp, 16             ; Clean up the pushed arguments from the stack
    mov [rel %1], rax       ; Move returned pointer into specified var
%endmacro

%macro FREE_ARRAY 1
    ALIGN_STACK
    mov rax, %1
    call array_free
    add rsp, 16
%endmacro

%macro SET_ARRAY_ELEM 4
    ; Parameters: %1 = array name, %2 = index, %3 = value, %4 = elem_size
    ; Compute offset = index * elem_size. For small constant elem_size, NASM will optimize.
    mov rdx, %2
    imul rdx, %4
    add rdx, %1

    %if %4 = CHAR_
        mov BYTE [rdx], %3
    %elif %4 = SHORT_
        mov WORD [rdx], %3
    %elif %4 = INT_
        mov DWORD [edx], %3
    %elif %4 = LONG_
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
    %if %4 = CHAR_
        mov %3, BYTE [rdx]
    %elif %4 = SHORT_
        mov %3, WORD [rdx]
    %elif %4 = INT_
        mov %3, DWORD [edx]
    %elif %4 = LONG_
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
    %if %3 = CHAR_                
        movzx rsi, BYTE [rdx]
    %elif %3 = SHORT_               
        movzx rsi, WORD [rdx]
    %elif %3 = INT_              
        mov esi, DWORD [edx]
    %elif %3 = LONG_              
        mov rsi, QWORD [rdx]
    %else
        ; Invalid element size
        ; Must be 1, 2, 4, or 8
        mov rdi, 2
        call exit
    %endif
    lea rdi, [rel fmt_int]
    xor rax, rax
    call printf

    mov rax, rbx    ; restore rax
%endmacro


; array_malloc
;
; args:
;   [rbp + 00]: stack pointer
;   [rbp + 08]: return pointer
;   [rbp + 16]: size (pushed by caller)
array_malloc:
    enter

    ; malloc(size)
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

; array_free
;
; args:
;   [rbp + 00]: stack pointer
;   [rbp + 08]: return pointer
;   [rbp + 16]: array pointer
array_free:
    enter
    mov rdi, [ARG1]
    call free
    leave

%define ELEM_SIZE 1

main:
    ; Create a new array: `array = malloc(count * sizeof(T));`
    ; There is no bounds checking, so memory is always volatile.
    NEW_ARRAY array_ptr, array_count, ELEM_SIZE

    ; Set array elements: `array[index] = value;`
    SET_ARRAY_ELEM array_ptr, 0, 5, ELEM_SIZE 
    SET_ARRAY_ELEM array_ptr, 1, 10, ELEM_SIZE
    SET_ARRAY_ELEM array_ptr, 2, 15, ELEM_SIZE
    SET_ARRAY_ELEM array_ptr, 3, 50, ELEM_SIZE

    ; Print array elements: `printf("%d\n", array[index]);`
    PRINT_ARRAY_ELEM array_ptr, 0, ELEM_SIZE
    PRINT_ARRAY_ELEM array_ptr, 1, ELEM_SIZE
    PRINT_ARRAY_ELEM array_ptr, 2, ELEM_SIZE
    PRINT_ARRAY_ELEM array_ptr, 3, ELEM_SIZE

    FREE_ARRAY array_free

    ; Exit
    mov rdi, 0
    call exit
