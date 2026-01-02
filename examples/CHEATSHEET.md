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
> `n` below represents the byte-size of each stack element which varies depending on the operating system (e.g. 32-bit vs. 64-bit).
> Values can be:
> | Arch | Size |
> | --- | --- |
> | `x86-32` | 4 |
> | `win32` | 4 |
> | `x86-64` | 8 |
> | `x64` | 8 |

| Offset | Purpose |
| --- | --- |
| `[rbp + (n * 0)]`  | Stack pointer  |
| `[rbp + (n * 1)]`  | Return pointer  |
| `[rbp + (n * 2)]`  | Arg 1  |
| `[rbp + (n * 3)]`  | Arg 2  |
| `[rbp + (n * 4)]`  | Arg 3  |


```asm

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