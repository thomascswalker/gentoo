#include <stddef.h>
#include <string.h>

#include "assert.h"
#include "ast.h"
#include "log.h"
#include "reg.h"
#include "stdlib.h"
#include "targets.h"
#include "x86_64.h"

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
bool g_in_main = false;
bool g_in_function = false;
scope_t* g_scope = NULL;
scope_t* g_global_scope = NULL;
ptrdiff_t g_stack_offset = 0;

// Allocate a new scope inheriting the parent's bindings.
scope_t* scope_create(scope_t* parent)
{
    scope_t* scope = (scope_t*)calloc(1, sizeof(scope_t));
    scope->parent = parent;
    scope->capacity = 8;
    scope->symbols = (symbol_t*)calloc(scope->capacity, sizeof(symbol_t));
    return scope;
}

void scope_destroy(scope_t* scope)
{
    if (!scope)
    {
        return;
    }
    free(scope->symbols);
    free(scope);
}

void scope_push()
{
    assert(g_scope != NULL, "Cannot push scope with no parent.");
    g_scope = scope_create(g_scope);
}

void scope_pop()
{
    assert(g_scope != NULL, "No scope to pop.");
    scope_t* current = g_scope;
    assert(current->parent != NULL, "Cannot pop the global scope.");
    g_scope = current->parent;
    scope_destroy(current);
}

symbol_t* scope_lookup_shallow(scope_t* scope, const char* name)
{
    if (!scope)
    {
        return NULL;
    }
    for (size_t i = 0; i < scope->count; i++)
    {
        if (strcmp(scope->symbols[i].name, name) == 0)
        {
            return &scope->symbols[i];
        }
    }
    return NULL;
}

symbol_t* scope_lookup(scope_t* scope, const char* name)
{
    scope_t* current = scope;
    while (current)
    {
        symbol_t* symbol = scope_lookup_shallow(current, name);
        if (symbol)
        {
            return symbol;
        }
        current = current->parent;
    }
    return NULL;
}

symbol_t* scope_add_symbol(scope_t* scope, const char* name, symbol_kind_t kind)
{
    assert(scope != NULL, "Scope cannot be NULL when adding a symbol.");
    if (scope->count >= scope->capacity)
    {
        size_t new_capacity = scope->capacity ? scope->capacity * 2 : 8;
        scope->symbols =
            (symbol_t*)realloc(scope->symbols, new_capacity * sizeof(symbol_t));
        memset(scope->symbols + scope->capacity, 0,
               (new_capacity - scope->capacity) * sizeof(symbol_t));
        scope->capacity = new_capacity;
    }

    symbol_t* symbol = &scope->symbols[scope->count++];
    symbol->name = name;
    symbol->kind = kind;
    symbol->stack_offset = 0;
    return symbol;
}

ptrdiff_t allocate_stack_slot()
{
    assert(g_in_function,
           "Stack slots can only be allocated inside functions.");
    g_stack_offset += 8;
    B_TEXT("\tsub rsp, 8\n");
    return -g_stack_offset;
}

symbol_t* symbol_define_global(const char* name)
{
    assert(g_global_scope != NULL, "Global scope is not initialized.");
    symbol_t* existing = scope_lookup_shallow(g_global_scope, name);
    assert(existing == NULL, "Global symbol %s already defined.", name);
    return scope_add_symbol(g_global_scope, name, SYMBOL_GLOBAL);
}

symbol_t* symbol_define_local(const char* name)
{
    assert(g_scope != NULL, "Current scope is not set.");
    assert(g_scope != g_global_scope,
           "Local declarations require a function scope.");
    symbol_t* existing = scope_lookup_shallow(g_scope, name);
    assert(existing == NULL, "Symbol %s already defined in this scope.", name);

    symbol_t* symbol = scope_add_symbol(g_scope, name, SYMBOL_LOCAL);
    symbol->stack_offset = allocate_stack_slot();
    return symbol;
}

symbol_t* symbol_resolve(const char* name)
{
    symbol_t* symbol = scope_lookup(g_scope, name);
    assert(symbol != NULL, "Undefined symbol: %s", name);
    return symbol;
}

// Pre-scan the top-level bodies so globals exist before functions reference
// them.
void collect_global_symbols(ast* node)
{
    if (!node)
    {
        return;
    }

    assert(node->type == AST_PROGRAM,
           "Expected PROGRAM node when collecting globals, got %s",
           ast_to_string(node->type));

    ast_program* program = &node->data.program;
    for (int i = 0; i < program->count; i++)
    {
        ast* body_node = program->body[i];
        if (!body_node || body_node->type != AST_BODY)
        {
            continue;
        }

        ast_body* body = &body_node->data.body;
        for (int j = 0; j < body->count; j++)
        {
            ast* statement = body->statements[j];
            if (!statement || statement->type != AST_ASSIGN)
            {
                continue;
            }

            ast* lhs = statement->data.assign.lhs;
            if (lhs && lhs->type == AST_DECLVAR)
            {
                char* name = lhs->data.declvar.identifier->data.identifier.name;
                if (scope_lookup_shallow(g_global_scope, name) == NULL)
                {
                    scope_add_symbol(g_global_scope, name, SYMBOL_GLOBAL);
                }
            }
        }
    }
}

void x86_epilogue(bool emit_ret)
{
    B_TEXT("\tmov rsp, rbp\n");
    B_TEXT("\tpop rbp\n");
    if (emit_ret)
    {
        B_TEXT("\tret\n");
    }
}

/*
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
            // buffer_printf(g_text, "\txor %s, %s\n", reg->name, reg->name);
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

static void x86_block(ast* node)
{
    assert(node->type == AST_BLOCK, "Expected BLOCK node, got %s",
           ast_to_string(node->type));

    // Each block introduces a fresh scope to keep locals isolated.
    scope_push();
    ast_block* block = &node->data.block;
    for (int i = 0; i < block->count; i++)
    {
        x86_statement(block->statements[i]);
    }
    scope_pop();
}

void x86_declfn(ast* node)
{
    ENTER(DECLFN);

    char* name = node->data.declfn.identifier->data.identifier.name;

    bool prev_in_function = g_in_function;
    ptrdiff_t prev_stack_offset = g_stack_offset;
    bool prev_is_main = g_in_main;

    g_in_function = true;
    g_stack_offset = 0;
    g_in_main = (strcmp(name, "main") == 0);

    B_TEXT("global %s\n", name);
    B_TEXT("%s:\n", name);
    // Standard prologue so locals can be addressed relative to RBP.
    B_TEXT("\tpush rbp\n");
    B_TEXT("\tmov rbp, rsp\n");

    x86_block(node->data.declfn.block);

    g_in_function = prev_in_function;
    g_stack_offset = prev_stack_offset;
    g_in_main = prev_is_main;
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
    symbol_t* symbol = NULL;

    switch (lhs->type)
    {
    // If it's a new variable, declare it
    case AST_DECLVAR:
        name = lhs->data.declvar.identifier->data.identifier.name;
        if (g_in_function)
        {
            // Locals consume stack slots inside the current function.
            symbol = symbol_define_local(name);
        }
        else
        {
            symbol = scope_lookup_shallow(g_global_scope, name);
            if (!symbol)
            {
                symbol = symbol_define_global(name);
            }
            x86_declvar(lhs);
        }
        break;
    // Otherwise obtain the existing variable name
    case AST_IDENTIFIER:
        name = lhs->data.identifier.name;
        symbol = symbol_resolve(name);
        break;
    }

    assert(symbol != NULL, "Failed to resolve symbol for %s", name);

    if (symbol->kind == SYMBOL_GLOBAL)
    {
        B_TEXT("\tmov [%s], %s\n", name, rhs_reg);
    }
    else
    {
        // Stack locals are addressed relative to RBP.
        B_TEXT("\tmov [rbp%+td], %s\n", symbol->stack_offset, rhs_reg);
    }

    if (rhs->type != AST_CALL)
    {
        register_release();
    }

    EXIT(ASSIGN);
}

void x86_return(ast* node)
{
    ENTER(RET);

    if (g_in_main)
    {
        // Ensure register 5 (rdi) is not locked. It's required for
        // returning the exit code in Linux, where RDI is the register
        // for the first argument of a function (in this case, syscall 60
        // which is Linux's exit syscall).
        assert(!g_registers[5].locked,
               "%s cannot be locked at the point of return.", "RDI");
    }
    ast* rhs = node->data.ret.node;
    char* rhs_reg;

    switch (rhs->type)
    {
    case AST_BINOP:
    case AST_CONSTANT:
    case AST_IDENTIFIER:
    case AST_CALL:
        rhs_reg = x86_expr(rhs);
        break;
    default:
        assert(false,
               "Invalid right-hand type for RETURN: %d. Wanted one of "
               "[BINOP, CONSTANT, IDENTIFIER, CALL].",
               rhs->type);
    }

    // If this is NOT the main function, just do a normal return.
    // This will return the value in RAX.
    if (!g_in_main)
    {
        // Move the result into RAX
        B_TEXT("\tmov rax, %s\n", rhs_reg);
        if (rhs->type != AST_CALL)
        {
            register_release();
        }
        x86_epilogue(true);
    }
    // Otherwise we need to exit the program. Move RAX into RDI,
    // which is the first argument for the linux exit syscall.
    else
    {
        B_TEXT("\tmov rdi, %s\n", rhs_reg);
        if (rhs->type != AST_CALL)
        {
            register_release();
        }
        x86_epilogue(false);
        x86_syscall(X86_EXIT);
    }
    EXIT(RET);
}

char* x86_call(ast* node)
{
    ENTER(CALL);
    // The return value is always stored in RAX
    char* reg = RAX;
    // Call the function
    B_TEXT("\tcall %s\n", node->data.call.identifier->data.identifier.name);
    EXIT(CALL);
    return reg;
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
        {
            symbol_t* symbol = symbol_resolve(node->data.identifier.name);
            if (symbol->kind == SYMBOL_GLOBAL)
            {
                B_TEXT("\tmov %s, [%s]\n", reg, symbol->name);
            }
            else
            {
                // Load local values via their recorded stack offset.
                B_TEXT("\tmov %s, [rbp%+td]\n", reg, symbol->stack_offset);
            }
        }
        break;
    case AST_CALL:
        reg = x86_call(node);
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
    case AST_DECLFN:
        x86_declfn(node);
        break;
    case AST_BLOCK:
        x86_block(node);
        break;
    case AST_RETURN:
        x86_return(node);
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

    // Reset scope state for this emission run.
    scope_destroy(g_global_scope);
    g_global_scope = scope_create(NULL);
    g_scope = g_global_scope;
    g_in_function = false;
    g_stack_offset = 0;

    collect_global_symbols(node);

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

    scope_destroy(g_global_scope);
    g_global_scope = NULL;
    g_scope = NULL;
}