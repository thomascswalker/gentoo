#include "x86_64.h"

#include "ast.h"
#include "log.h"
#include "targets.h"

#define STRING asciz
#define EXPAND(x) #x
#define NEW_LINE "\\n"

#define RDI "%rdi"

#define SYMBOL(symbol, type, text)                                             \
    B_DATA("\t%s: .%s %s\n", #symbol, #type, EXPAND(text));                    \
    B_DATA("\t%s_len = . - %s\n", #symbol, #symbol)
#define SYMBOL2(symbol, text)                                                  \
    B_DATA("\t%s: .asciz \"%s\"\n", symbol, text);                             \
    B_DATA("\t%s_len = . - %s\n", symbol, symbol)

#define MOVQ(value, reg) B_TEXT("\tmovq $%s, %%%s\n", #value, #reg)

#define LEAQ(symbol, reg) B_TEXT("\tleaq %s(%%rip), %%%s\n", #symbol, #reg)

#define ZERO_REG(reg) B_TEXT("\txor %%%s, %%%s\n", #reg, #reg)

// Macros for variables (not literals)
#define MOVQ_VI_MR(value, reg) B_TEXT("\tmovq $%d, %s\n", value, reg)
#define EXIT_CODE(code) B_TEXT("\tmovq $%d, %%rdi\n", code)

// Macro for calling a system interrupt via code
#define SYSCALL(code)                                                          \
    MOVQ(code, rax);                                                           \
    B_TEXT("\tsyscall\n");

// Macro for outputting to stdout
#define STDOUT(symbol)                                                         \
    MOVQ(1, rdi);                                                              \
    LEAQ(symbol, rsi);                                                         \
    MOVQ(symbol##_len, rdx);                                                   \
    SYSCALL(1)

void target_x86_64(ast* node)
{
    // Initialize sections
    B_DATA(".section .data\n");

    // Data
    char* test_string = "test strisdflksjdlfkjdng!" NEW_LINE;
    SYMBOL2("fmt", test_string);

    // Entry point
    B_TEXT("\n");
    B_TEXT(".section .text\n");
    B_TEXT("\t.globl _start\n");
    B_TEXT("\n");
    B_TEXT("_start:\n");

    // Code
    STDOUT(fmt);

    // Exit code
    int exit_code = 1;
    EXIT_CODE(exit_code);

    // Exit
    SYSCALL(60);
}