# NASM x86(-64) Assembly Cheatsheet

## Requirements

- NASM
- GCC

### Additional Requirements on Windows

- WSL2

## Functions

### Declaration

```asm
global my_function

; ...

my_function:
    enter       ; Initialize inner scope
    ; ...
    leave       ; Restore to outer scope
```

### Calling functions with arguments

Arguments are pushed onto the stack in reverse order. Calling this function in C:

```c
my_function(1, 2, 3);
```

Would be the equivalent of this assembly:

```asm
my_function:
    ; ...

push QWORD 3
push QWORD 2
push QWORD 1
call my_function
```

### Accessing function arguments within a function

Because arguments are passed on the stack, they need to be accessed through the stack. The stack inside a function, once `enter` has been used, looks like:

> [!IMPORTANT]
> `n` below represents the WORD size which varies depending on the operating system (e.g. 32-bit vs. 64-bit).
> Values can be:
> | Arch | WORD Size |
> | --- | --- |
> | `x86-32` | 4 |
> | `x86-64` | 8 |

| Offset | Purpose |
| --- | --- |
| `[rbp + (n * 0)]`  | Stack pointer |
| `[rbp + (n * 1)]`  | Return pointer  |
| `[rbp + (n * 2)]`  | Arg 1  |
| `[rbp + (n * 3)]`  | Arg 2  |
| `[rbp + (n * 4)]`  | Arg 3  |


```asm
my_function:
    enter
    mov rdi, [rbp + 16] ; Move argument 1 into RDI
    ; ...
    leave
```

### Prologue/Epilogues (`enter` and `leave` macros)

When **entering** a function, the convention is to push the current base pointer onto the stack, then move the current stack pointer into the base pointer. 


```asm
; Prologue
push rbp        ; Saves the caller's base pointer onto the stack
mov rbp, rsp    ; Sets the current base pointer to the current stack pointer
```

When **exiting** a function, the convention is reversed: move the current base pointer into the stack pointer, pop the newest element on the stack into the base pointer, then return.

```asm
; Epilogue
mov rsp, rbp    ; Restore stack pointer from frame pointer
pop rbp         ; Restores the caller's base pointer
ret             ; Returns from the function
```

## External Functions (from C)

### Including external functions

> [!WARNING]
> Using external C functions requires linking with the C standard library. While this can be done with `ld`, ultimately it's easiest to link with `gcc`.

```asm
extern printf
```

### Calling external functions

External C functions may pass arguments on the stack, but they also
may expect arguments are passed in registers.

```asm
global main
extern printf                           ; Include `printf` from `stdlib`

section .data
    my_string: db "Number: %d\n", 10, 0 ; Format string

section .text
main:
    mov rdi, [rel my_string]            ; Move into argument 1
    mov rsi, 5                          ; Move into argument 2
    call printf                         ; Print 'Number: 5'
```

## Macros

### Single-line

```asm
%define MY_MACRO xor rax, rax   ; Expands to `xor rax, rax`
```

### Multi-line

```asm
%macro MY_MACRO 1   ; Identifier, argument count
    mov rax, %1     ; Use argument
%endmacro
```