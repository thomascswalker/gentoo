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

void x86_comment(char* text)
{
    buffer_printf(g_text, "; %s\n", text);
}

/*
 *  `mov <value>, <reg>`
 */
void x86_mov(int value, char* reg)
{
    log_info("Moving %d into %s", value, reg);
    buffer_printf(g_text, "\tmov %s, %d\n", reg, value);
}

void x86_binop(ast* node)
{
    log_info("Emitting x86_64 binop...");

    ast_binop* binop = &node->data.binop;

    // Emit left hand side
    if (binop->lhs->type == AST_CONSTANT)
    {
        int lhs_value = binop->lhs->data.constant.value;
        x86_mov(lhs_value, RBX);
    }
    // Emit right hand side
    if (binop->rhs->type == AST_CONSTANT)
    {
        int rhs_value = binop->rhs->data.constant.value;
        x86_mov(rhs_value, RCX);
    }

    // Emit the op
    switch (binop->op)
    {
    case BIN_ADD:
    {
        log_info("Adding %s to %s", RBX, RCX);
        buffer_printf(g_text, "\tlea %s, [%s+%s]\n", RAX, RBX, RCX);
        break;
    }
    default:
        break;
    }
    log_info("Completed emitting x86_64 binop");
}

void x86_declvar(ast* node)
{
    char* name = node->data.declvar.identifier->data.identifier.name;
    buffer_printf(g_data, "\t%s: dw %d\n",
                  node->data.declvar.identifier->data.identifier.name, 0);
}

void x86_assign(ast* node)
{
    x86_comment("Enter assign");

    // Emit the right hand side first (fully processing any expressions)
    ast* rhs = node->data.assign.rhs;
    x86_statement(rhs);

    // Assume the left hand side is an identifier
    ast* lhs = node->data.assign.lhs;
    char* name = NULL;

    // If it's a new variable, declare it
    if (lhs->type == AST_DECLVAR)
    {
        name = lhs->data.declvar.identifier->data.identifier.name;
        x86_declvar(lhs);
    }
    // Otherwise obtain the existing variable name
    else
    {
        name = lhs->data.identifier.name;
    }

    // If it's a constant value, store the value in RAX first
    if (rhs->type == AST_CONSTANT)
    {
        buffer_printf(g_text, "\tmov %s, %d\n", RAX, rhs->data.constant.value);
    }
    // Otherwise assume it's an identifier
    else if (rhs->type == AST_IDENTIFIER)
    {
        buffer_printf(g_text, "\tmov %s, [%s]\n", RAX,
                      rhs->data.identifier.name);
    }
    // Mov RAX into [<name>]
    buffer_printf(g_text, "\tmov [%s], %s\n", name, RAX);

    x86_comment("Exit assign");
}

void x86_return(ast* node)
{
}

void x86_statement(ast* node)
{
    log_info("Emitting x86_64 statement...");
    switch (node->type)
    {
    case AST_ASSIGN:
        x86_assign(node);
        break;
    case AST_BINOP:
        x86_comment("Enter binop");
        x86_binop(node);
        x86_comment("Exit binop");
        break;
    case AST_DECLVAR:
        x86_declvar(node);
        break;
    case AST_RETURN:
        x86_return(node);
        break;
    default:
        break;
    }
    log_info("Completed emitting x86_64 statement");
}

void x86_body(ast* node)
{
    assert(node->type == AST_BODY, "Wanted node type BODY, got %s", node->type);
    log_info("Emitting x86_64 body...");
    for (size_t i = 0; i < node->data.body.count; i++)
    {
        log_debug("Statement %d", i);
        ast* statement = node->data.body.statements[i];
        x86_statement(statement);
    }
    log_info("Completed emitting x86_64 body");
}

void target_x86(ast* node)
{
    assert(node->type == AST_PROGRAM, "Wanted node type PROGRAM, got %s",
           node->type);

    // make all symbol references RIP-relative by default
    buffer_printf(g_global, "default rel\n");

    // Initialize sections
    B_BSS("section .bss\n");
    B_DATA("section .data\n");
    B_TEXT("section .text\n");

    // Entry point
    const char* entry_point = "main";
    B_TEXT("global %s\n", entry_point);
    B_TEXT("%s:\n", entry_point);

    // Code
    for (size_t i = 0; i < node->data.program.count; i++)
    {
        log_debug("Body %d", i);
        ast* body = node->data.program.body[i];
        x86_body(body);
    }

    // Move value of RAX into RDI (assign the exit code)
    buffer_printf(g_text, "\tmov %s, [%s]\n", RDI, "var1");

    // Exit, syscall 60 on Linux
    SYSCALL(60);
}