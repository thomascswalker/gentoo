## Functions

### Prologue/Epilogues

```asm
; Prologue
push rbp        ; Saves the caller's base pointer onto the stack
mov rbp, rsp    ; Sets the current base pointer to the current stack pointer

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