#include "x86_64.h"
#include "assert.h"
#include "ast.h"
#include "log.h"
#include "reg.h"
#include "stdlib.h"
#include "targets.h"

reg_t g_registers[REG_COUNT] = {
    {"rax", false}, //
    {"rbx", false}, //
    {"rcx", false}, //
    {"rdx", false}, //
    {"rsi", false}, //
    {"rdi", false}, //
    {"r8", false},  //
    {"r9", false},  //
    {"r10", false}, //
    {"r11", false}, //
    {"r12", false}, //
    {"r13", false}, //
    {"r14", false}, //
    {"r15", false}, //
};
size_t g_lock_count = 0;
size_t g_unlock_count = 0;
bool g_is_main = false;

/* \
Assert that the unlock count is less than or equal to the lock count. If the
unlock count is greater than lock count, there's a mismatch with how many
registers have been allocated and this will probably cause a SEGFAULT.
*/
void register_assert()
{
    assert(g_unlock_count <= g_lock_count,
           "Unlock can never be greater than lock!: Lock:%d > Unlock:%d",
           g_lock_count, g_unlock_count);
}

/*
 * Returns the next available register. Registers are prioritized in order
 * in the `g_registers` array. If no register is available, return a NULL
 * pointer.
 */
char* register_get()
{
    for (int i = 0; i < REG_COUNT; i++)
    {
        reg_t* reg = &g_registers[i];
        if (reg->locked == false)
        {
            g_lock_count++;
            register_assert();
            reg->locked = true;

            return reg->name;
        }
    }
    return NULL;
}

/*
 * Release the most-recently retrieved register.
 */
char* register_release()
{
    // Release the most-recently locked register (LIFO). Scan from the end
    // so that we free the last locked register first.
    for (int i = REG_COUNT - 1; i >= 0; i--)
    {
        reg_t* reg = &g_registers[i];
        if (reg->locked == true)
        {
            g_unlock_count++;
            register_assert();
            reg->locked = false;
            buffer_printf(g_text, "\txor %s, %s\n", reg->name, reg->name);
            return reg->name;
        }
    }
    return NULL;
}

/* Emits a simple comment line. */
void x86_comment(char* text)
{
    B_TEXT("; %s\n", text);
}

void x86_syscall(int code)
{
    buffer_printf(g_text, "\tmov %s, %d\n", RAX, code);
    buffer_printf(g_text, "\tsyscall\n");
}

#define ENTER(name) log_debug("Entering " #name)
// x86_comment("Entering " #name);
#define EXIT(name) log_debug("Exiting " #name)
// x86_comment("Exiting " #name);

/*
 *  `mov <value>, <reg>`
 */
void x86_mov(int value, char* reg)
{
    log_info("Moving %d into %s", value, reg);
    B_TEXT("\tmov %s, %d\n", reg, value);
}

char* x86_binop(ast* node)
{
    ENTER(BINOP);

    ast_binop* binop = &node->data.binop;

    // Reserve a register for the output
    char* out_reg = register_get();
    char* lhs = x86_expr(binop->lhs);
    char* rhs = x86_expr(binop->rhs);

    // Emit the corresponding op
    switch (binop->op)
    {
    case BIN_ADD:
    {
        // Compute out_reg = lhs + rhs (preserve left-to-right order)
        B_TEXT("\tmov %s, %s\n", out_reg, lhs);
        B_TEXT("\tadd %s, %s\n", out_reg, rhs);
        // Release rhs
        register_release();
        // Release lhs
        register_release();

        break;
    }
    case BIN_SUB:
    {
        // Compute out_reg = lhs - rhs (preserve left-to-right order)
        B_TEXT("\tmov %s, %s\n", out_reg, lhs);
        B_TEXT("\tsub %s, %s\n", out_reg, rhs);
        // Release rhs
        register_release();
        // Release lhs
        register_release();

        break;
    }
    case BIN_MUL:
    {
        // Move lhs into the output register, then multiply by rhs
        B_TEXT("\tmov %s, %s\n", out_reg, lhs);
        B_TEXT("\timul %s, %s\n", out_reg, rhs);
        // Release rhs
        register_release();
        // Release lhs
        register_release();

        break;
    }
    case BIN_DIV:
    case BIN_EQ:
        assert(false, "BINOP %s not implemented yet.",
               binop_to_string(binop->op));
    default:
        break;
    }

    EXIT(BINOP);

    return out_reg;
}

void x86_declvar(ast* node)
{
    ENTER(DECLVAR);
    char* name = node->data.declvar.identifier->data.identifier.name;
    B_DATA("\t%s: dq %d\n", name, 0);
    EXIT(DECLVAR);
}

void x86_declfn(ast* node)
{
    ENTER(DECLFN);

    char* name = node->data.declfn.identifier->data.identifier.name;

    if (strcmp(name, "main") == 0)
    {
        g_is_main = true;
    }

    B_TEXT("global %s\n", name);
    B_TEXT("%s:\n", name);

    ast_block* block = &node->data.declfn.block->data.block;
    for (size_t i = 0; i < block->count; i++)
    {
        x86_statement(block->statements[i]);
    }

    if (g_is_main)
    {
        g_is_main = false;
    }
    EXIT(DECLFN);
}

void x86_assign(ast* node)
{
    ENTER(ASSIGN);

    // Emit the right hand side first (fully processing any expressions)
    ast* rhs = node->data.assign.rhs;
    char* rhs_reg = x86_expr(rhs);

    // Assume the left hand side is an identifier
    ast* lhs = node->data.assign.lhs;
    char* name = NULL;

    switch (lhs->type)
    {
    // If it's a new variable, declare it
    case AST_DECLVAR:
        name = lhs->data.declvar.identifier->data.identifier.name;
        x86_declvar(lhs);
        break;
    // Otherwise obtain the existing variable name
    case AST_IDENTIFIER:
        name = lhs->data.identifier.name;
        break;
    }

    // If it's a constant value, store the value in <rhs_reg> first
    // Mov <rhs_reg> into [<name>]
    B_TEXT("\tmov [%s], %s\n", name, rhs_reg);

    register_release();

    EXIT(ASSIGN);
}

/* Currently only support root-level return (main program exit). */
void x86_sysexit(ast* node)
{
    ENTER(SYSEXIT);

    // Ensure register 5 (rdi) is not locked. It's required for
    // returning the exit code in Linux, where RDI is the register
    // for the first argument of a function (in this case, syscall 60
    // which is Linux's exit syscall).
    assert(!g_registers[5].locked,
           "%s cannot be locked at the point of return.", "RDI");

    // Move value of RAX into RDI (assign the exit code)
    ast* rhs = node->data.ret.node;
    char* rhs_reg;

    switch (rhs->type)
    {
    // Process the binary operation and then move its output register
    // into RDI.
    case AST_BINOP:
        rhs_reg = x86_binop(rhs);
        B_TEXT("\tmov %s, [%s]\n", RDI, rhs_reg);
        register_release();
        break;

    // Don't need to occupy a register for this, just move the constant value
    // into RDI.
    case AST_CONSTANT:
        B_TEXT("\tmov %s, %d\n", RDI, rhs->data.constant.value);
        break;

    // Don't need to occupy a register for this, just move the identifier's
    // value into RDI.
    case AST_IDENTIFIER:
        B_TEXT("\tmov %s, [%s]\n", RDI, rhs->data.identifier.name);
        break;
    default:
        assert(false,
               "Invalid right-hand type for RETURN: %d. Wanted one of "
               "[BINOP, CONSTANT, IDENTIFIER].",
               rhs->type);
    }

    // Exit, syscall 60 on Linux
    x86_syscall(X86_EXIT);
    EXIT(SYSEXIT);
}

void x86_return(ast* node)
{
    ENTER(RET);

    // Ensure register 5 (rdi) is not locked. It's required for
    // returning the exit code in Linux, where RDI is the register
    // for the first argument of a function (in this case, syscall 60
    // which is Linux's exit syscall).
    assert(!g_registers[5].locked,
           "%s cannot be locked at the point of return.", "RDI");

    // Move value of RAX into RDI (assign the exit code)
    ast* rhs = node->data.ret.node;
    char* rhs_reg;

    switch (rhs->type)
    {
    // Process the binary operation and then move its output register
    // into RDI.
    case AST_BINOP:
        rhs_reg = x86_binop(rhs);
        B_TEXT("\tmov %s, [%s]\n", RAX, rhs_reg);
        register_release();
        break;

    // Don't need to occupy a register for this, just move the constant value
    // into RDI.
    case AST_CONSTANT:
        B_TEXT("\tmov %s, %d\n", RAX, rhs->data.constant.value);
        break;

    // Don't need to occupy a register for this, just move the identifier's
    // value into RDI.
    case AST_IDENTIFIER:
        B_TEXT("\tmov %s, [%s]\n", RAX, rhs->data.identifier.name);
        break;
    default:
        assert(false,
               "Invalid right-hand type for RETURN: %d. Wanted one of "
               "[BINOP, CONSTANT, IDENTIFIER].",
               rhs->type);
    }

    B_TEXT("\tret\n");
    EXIT(RET);
}

char* x86_expr(ast* node)
{
    ENTER(EXPR);

    // The resultant register of this expression. This register contains
    // the final computed value of the expression.
    char* reg = NULL;

    switch (node->type)
    {
    case AST_BINOP:
        // Emit a binary operation (+, -, *, /, etc.)
        reg = x86_binop(node);
        break;
    case AST_CONSTANT:
        // Get a new register to store the constant
        reg = register_get();
        // Move the constant into this register
        B_TEXT("\tmov %s, %d\n", reg, node->data.constant.value);
        break;
    case AST_IDENTIFIER:
        // Get a new register to store the identifier's value
        reg = register_get();
        // Move the value of the identifier into the register
        B_TEXT("\tmov %s, [%s]\n", reg, node->data.identifier.name);
        break;
    default:
        break;
    }
    EXIT(EXPR);

    return reg;
}

void x86_statement(ast* node)
{
    ENTER(STMT);
    switch (node->type)
    {
    case AST_ASSIGN:
        x86_assign(node);
        break;
    case AST_DECLVAR:
        x86_declvar(node);
        break;
    case AST_DECLFN:
        x86_declfn(node);
        break;
    case AST_RETURN:
        if (g_is_main)
        {
            x86_sysexit(node);
        }
        else
        {
            x86_return(node);
        }
        break;
    default:
        break;
    }
    EXIT(STMT);
}

void x86_body(ast* node)
{
    assert(node->type == AST_BODY, "Wanted node type BODY, got %s", node->type);
    ENTER(BODY);
    for (size_t i = 0; i < node->data.body.count; i++)
    {
        ast* statement = node->data.body.statements[i];
        x86_statement(statement);
    }
    EXIT(BODY);
}

/* The main entry point for emitting ASM code from the AST. */
void target_x86(ast* node)
{
    assert(node->type == AST_PROGRAM, "Wanted node type PROGRAM, got %s",
           node->type);

    // Make all symbol references RIP-relative by default
    // https://www.nasm.us/doc/nasm08.html#section-8.2.1
    buffer_printf(g_global, "default rel\n");

    // Initialize sections
    B_BSS("section .bss\n");
    B_DATA("section .data\n");
    B_TEXT("section .text\n");

    // Code
    for (size_t i = 0; i < node->data.program.count; i++)
    {
        ast* body = node->data.program.body[i];
        x86_body(body);
    }
}