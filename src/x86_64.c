#include "x86_64.h"
#include "assert.h"
#include "ast.h"
#include "log.h"
#include "stdlib.h"
#include "targets.h"

#define STRING asciz
#define EXPAND(x) #x
#define NEW_LINE "\\n"

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

#define SYMBOL(symbol, type, text)                                             \
    B_DATA("\t%s: .%s %s\n", #symbol, #type, EXPAND(text));                    \
    B_DATA("\t%s_len = . - %s\n", #symbol, #symbol)
#define SYMBOL2(symbol, text)                                                  \
    B_DATA("\t" #symbol ": .asciz \"%s\"\n", text);                            \
    B_DATA("\t" #symbol "_len = . - " #symbol "\n")

#define ADD(a, b) B_TEXT("\tadd %s, %s\n", a, b)
#define LEAQ(symbol, reg) B_TEXT("\tlea %s(%%rip), %%%s\n", symbol, reg)
#define ZERO_REG(reg) B_TEXT("\txor %%%s, %%%s\n", reg, reg)

// Macros for variables (not literals)
#define MOVQ_VI_MR(value, reg) B_TEXT("\tmov %d, %s\n", value, reg)
#define EXIT_CODE(code) B_TEXT("\tmov %d, %%rdi\n", code)

// Macro for calling a system interrupt via code (use rax for syscall number)
#define SYSCALL(code)                                                          \
    B_TEXT("\tmov %s, %d\n", RAX, code);                                       \
    B_TEXT("\tsyscall\n");

// Macro for outputting to stdout
#define STDOUT(symbol)                                                         \
    B_TEXT("\tmov 1, %s\n", RDI);                                              \
    LEAQ(EXPAND(symbol), RSI);                                                 \
    B_TEXT("\tmov %s, %s\n", EXPAND(symbol##_len), RDX);                       \
    SYSCALL(1)

/*
 *  `movq <value>, <reg>`
 */
void emit_x86_64_mov(int value, char* reg)
{
    log_info("Moving %d into %s", value, reg);
    buffer_printf(g_text, "\tmov %s, %d\n", reg, value);
}

void emit_x86_64_binop(ast* node)
{
    log_info("Emitting x86_64 binop...");

    ast_binop* binop = &node->data.binop;
    if (binop->lhs->type == AST_CONSTANT)
    {
        int lhs_value = binop->lhs->data.constant.value;
        emit_x86_64_mov(lhs_value, RAX);
    }
    if (binop->rhs->type == AST_CONSTANT)
    {
        int rhs_value = binop->rhs->data.constant.value;
        emit_x86_64_mov(rhs_value, RBX);
    }

    switch (binop->op)
    {
    case BIN_ADD:
    {
        log_info("Adding %s to %s", RAX, RBX);
        buffer_printf(g_text, "\tadd %s, %s\n", RAX, RBX);
        break;
    }
    default:
        break;
    }
    log_info("Completed emitting x86_64 binop");
}

void emit_x86_64_statement(ast* node)
{
    log_info("Emitting x86_64 statement...");
    switch (node->type)
    {
    case AST_ASSIGN:
        emit_x86_64_statement(node->data.assign.lhs);
        emit_x86_64_statement(node->data.assign.rhs);
        break;
    case AST_BINOP:
        emit_x86_64_binop(node);
        break;
    default:
        break;
    }
    log_info("Completed emitting x86_64 statement");
}

void emit_x86_64_body(ast* node)
{
    assert(node->type == AST_BODY, "Wanted node type BODY, got %s", node->type);
    log_info("Emitting x86_64 body...");
    for (size_t i = 0; i < node->data.body.count; i++)
    {
        log_debug("Statement %d", i);
        ast* statement = node->data.body.statements[i];
        emit_x86_64_statement(statement);
    }
    log_info("Completed emitting x86_64 body");
}

void target_x86_64(ast* node)
{
    assert(node->type == AST_PROGRAM, "Wanted node type PROGRAM, got %s",
           node->type);

    // Initialize sections
    buffer_printf(g_global,
                  ".intel_syntax noprefix\n"); // Force intel syntax
    B_DATA(".section .data\n");

    // Entry point
    B_TEXT("\n");
    B_TEXT(".section .text\n");
    B_TEXT("\t.global _start\n");
    B_TEXT("\n");

    B_TEXT("_start:\n");

    // Code
    for (size_t i = 0; i < node->data.program.count; i++)
    {
        log_debug("Body %d", i);
        ast* body = node->data.program.body[i];
        emit_x86_64_body(body);
    }
    buffer_printf(g_text, "\tmov %s, %s\n", RDI, RAX);
    // STDOUT(fmt);

    // Exit
    log_info("Calling exit syscall (60).");
    SYSCALL(60);
}