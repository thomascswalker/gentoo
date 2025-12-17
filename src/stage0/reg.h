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

reg_t* register_get(char* name);

/**
 * Assert that the unlock count is less than or equal to the lock count. If the
 * unlock count is greater than lock count, there's a mismatch with how many
 * registers have been allocated and this will probably cause a SEGFAULT.
 */
void register_assert();

/**
 * Returns the next available register. Registers are prioritized in order
 * in the `g_registers` array. If no register is available, return a NULL
 * pointer.
 */
char* register_lock();

/**
 * Release the most-recently retrieved register.
 */
char* register_unlock();

#endif