global main
global array_new
global array_subscript_set

section .data
    byte_count: dq 0x100                            ; 64-bit

    fmt_int: db "INT: %ld", 10, 0      ; 64-bit integer format
    fmt_malloc_result: db "Allocated %ld bytes at %p", 10, 0

section .text

extern exit
extern malloc
extern realloc
extern free
extern printf

%define ALIGN_STACK push 0

%macro enter 0
    push rbp                    ; Saves the caller's base pointer onto the stack
    mov rbp, rsp                ; Sets the current base pointer to the current stack pointer
%endmacro

%macro leave 0
    mov rsp, rbp                ; Restore stack pointer from frame pointer
    pop rbp                     ; Restores the caller's base pointer
    ret                         ; Returns from the function
%endmacro

%define ARG1 rbp + 16           ; First argument 
%define ARG2 rbp + 24           ; Second argument
%define ARG3 rbp + 32           ; Third argument 

%macro PRINT_INT 1
    mov rsi, %1                 
    lea rdi, [rel fmt_int]    
    xor eax, eax                
    xor rax, rax
    call printf
%endmacro

%define ELEM_SIZE 1

%macro ARRAY_NEW 1
    ALIGN_STACK
    push QWORD [%1]
    call array_new
    add rsp, 16                 ; Clean up the pushed arguments from the stack
%endmacro

%macro SET_ARRAY_ELEM 3
    ; Parameters: %1 = array name, %2 = index, %3 = value
    mov BYTE [%1 + %2 * ELEM_SIZE], %3   ; Store the value at the calculated address
%endmacro

%macro GET_ARRAY_ELEM 3
    ; %1 = base ptr, %2 = index, %3 = destination register (64-bit)
    movzx %3, BYTE [%1 + %2 * ELEM_SIZE]
%endmacro

%macro PRINT_ARRAY_ELEM 2
    ; %1 = base ptr, %2 = index
    mov rbx, rax    ; save rax, by calling printf we will override it

    movzx rsi, BYTE [%1 + %2 * ELEM_SIZE]               
    lea rdi, [rel fmt_int]    
    xor eax, eax                
    xor rax, rax
    call printf

    mov rax, rbx    ; restore rax
%endmacro

main:
    ; Call array_new with size on stack, keep alignment (push padding)
    ARRAY_NEW byte_count

    SET_ARRAY_ELEM rax, 0, 5    ; array[0] = 5
    SET_ARRAY_ELEM rax, 1, 10   ; array[1] = 10
    SET_ARRAY_ELEM rax, 2, 15   ; array[1] = 10

    PRINT_ARRAY_ELEM rax, 0
    PRINT_ARRAY_ELEM rax, 1
    PRINT_ARRAY_ELEM rax, 2

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
