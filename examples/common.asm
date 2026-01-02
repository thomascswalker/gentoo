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
%define ARG4 rbp + 40           ; Fourth argument
%define ARG5 rbp + 48           ; Fifth argument
