#ifndef REG_H
#define REG_H

#include <stdbool.h>

#define REG_COUNT 14

// General purpose registers
#define RAX "rax"
#define RBX "rbx"
#define RCX "rcx"
#define RDX "rdx"
#define RSI "rsi"
#define RDI "rdi"
#define R8 "r8"
#define R9 "r9"
#define R10 "r10"
#define R11 "r11"
#define R12 "r12"
#define R13 "r13"
#define R14 "r14"
#define R15 "r15"

// Stack registers
#define RBP "rbp" // Snapshot of stack pointer
#define RSP "rsp" // Stack pointer

typedef struct reg_t
{
    char* name;
    bool locked;
} reg_t;

#endif