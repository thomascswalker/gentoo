section .data
    ; Define the format string with a newline and null terminator
    format_string: db "The number is %d", 10, 0
    ; Define the integer value to print
    number:         dd 42

section .text
    global main     ; Make main visible to the linker (gcc)
    extern printf   ; Declare printf as an external function

main:
    ; Standard function prologue (optional but good practice for stack management)
    push    rbp
    mov     rbp, rsp
    ; Align stack for printf call (push rbp makes it misaligned by 8, call printf pushes another 8, so it works out)

    ; 1. Load the address of the format string into the first argument register (RDI)
    mov     rdi, format_string
    
    ; 2. Load the value of the number into the second argument register (RSI)
    mov     rsi, [number]
    
    ; 3. Set AL to 0 to indicate no vector registers are used for arguments
    xor     rax, rax

    ; 4. Call the printf function
    call    printf

    ; Standard function epilogue
    pop     rbp
    
    ; Return 0 (standard exit code for main in C)
    mov     rax, 0
    ret