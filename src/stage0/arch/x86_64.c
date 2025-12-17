#include <stddef.h>
#include <string.h>

#include "ast.h"
#include "codegen.h"
#include "log.h"
#include "macros.h"
#include "reg.h"
#include "stdlib.h"
#include "x86_64.h"

#define BUILTIN_PRINT "print"
#define ENTER(name) log_debug("Entering " #name)
#define EXIT(name) log_debug("Exiting " #name)

codegen_t CODEGEN_X86_64 = {
    .ops =
        {
            .program = x86_program,
            .body = x86_body,
            .statement = x86_statement,
            .binop = x86_binop,
            .declfn = x86_declfn,
            .declvar = x86_declvar,
            .assign = x86_assign,
            .call = x86_call,
            .expr = x86_expr,
            .syscall = x86_syscall,
            .comment = x86_comment,
            .epilogue = x86_epilogue,
        },
    .type = X86_64,
};

bool g_in_main = false;
bool g_in_function = false;
scope_t* g_scope = NULL;
scope_t* g_global_scope = NULL;

/**
 * @brief Records an offset to the stack base.
 */
ptrdiff_t g_stack_offset = 0;

scope_t* scope_new(scope_t* parent)
{
    scope_t* scope = (scope_t*)calloc(1, sizeof(scope_t));
    scope->parent = parent;
    scope->capacity = 8;
    scope->symbols = (symbol_t*)calloc(scope->capacity, sizeof(symbol_t));
    return scope;
}

void scope_free(scope_t* scope)
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
    ASSERT(g_scope != NULL, "Cannot push scope with no parent.");
    g_scope = scope_new(g_scope);
}

void scope_pop()
{
    ASSERT(g_scope != NULL, "No scope to pop.");
    scope_t* current = g_scope;
    ASSERT(current->parent != NULL, "Cannot pop the global scope.");
    g_scope = current->parent;
    scope_free(current);
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

symbol_t* scope_add_symbol(scope_t* scope, const char* name, symbol_type_t type)
{
    ASSERT(scope != NULL, "Scope cannot be NULL when adding a symbol.");

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
    symbol->type = type;
    symbol->offset = 0;

    char* message = symbol_to_string(symbol);
    log_debug("New symbol: %s", message);
    free(message);
    return symbol;
}

ptrdiff_t allocate_stack_slot()
{
    ASSERT(g_in_function,
           "Stack slots can only be allocated inside functions.");
    g_stack_offset += 8;
    EMIT(SECTION_TEXT, "\tsub rsp, 8\n");
    return -g_stack_offset;
}

symbol_t* symbol_define_global(const char* name)
{
    ASSERT(g_global_scope != NULL, "Global scope is not initialized.");
    symbol_t* existing = scope_lookup_shallow(g_global_scope, name);
    ASSERT(existing == NULL, "Global symbol %s already defined.", name);
    return scope_add_symbol(g_global_scope, name, SYMBOL_GLOBAL);
}

symbol_t* symbol_define_local(const char* name)
{
    ASSERT(g_scope != NULL, "Current scope is not set.");
    ASSERT(g_scope != g_global_scope,
           "Local declarations require a function scope.");
    symbol_t* existing = scope_lookup_shallow(g_scope, name);
    ASSERT(existing == NULL, "Symbol %s already defined in this scope.", name);

    symbol_t* symbol = scope_add_symbol(g_scope, name, SYMBOL_LOCAL);
    symbol->offset = allocate_stack_slot();
    return symbol;
}

symbol_t* symbol_resolve(const char* name)
{
    symbol_t* symbol = scope_lookup(g_scope, name);
    ASSERT(symbol != NULL, "Undefined symbol: %s", name);
    return symbol;
}

void get_global_symbols(ast* node)
{
    if (!node)
    {
        return;
    }

    ASSERT(node->type == AST_PROGRAM,
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

void x86_epilogue(bool returns)
{
    EMIT(SECTION_TEXT, "\tmov rsp, rbp\n");
    EMIT(SECTION_TEXT, "\tpop rbp\n");
    if (returns)
    {
        EMIT(SECTION_TEXT, "\tret\n");
    }
}

void x86_prologue()
{
    EMIT(SECTION_TEXT, "\tpush rbp\n");
    EMIT(SECTION_TEXT, "\tmov rbp, rsp\n");
}

void x86_print()
{
    EMIT(SECTION_GLOBAL, "global _%s\n", BUILTIN_PRINT);
    EMIT(SECTION_TEXT, "_%s:\n", BUILTIN_PRINT);
    EMIT(SECTION_TEXT, "\tpush rbp\n");
    EMIT(SECTION_TEXT, "\tmov rbp, rsp\n");
    EMIT(SECTION_TEXT, "\tsub rsp, 48\n");
    EMIT(SECTION_TEXT, "\tmov eax, edi\n");
    EMIT(SECTION_TEXT, "\tmov BYTE [rbp-1], 0\n");
    EMIT(SECTION_TEXT, "\tlea rsi, [rbp-1]\n");
    EMIT(SECTION_TEXT, "\tcmp eax, 0\n");
    EMIT(SECTION_TEXT, "\tjne .convert\n");
    EMIT(SECTION_TEXT, "\tmov BYTE [rsi-1], '0'\n");
    EMIT(SECTION_TEXT, "\tlea rsi, [rsi-1]\n");
    EMIT(SECTION_TEXT, "\tmov edx, 1\n");
    EMIT(SECTION_TEXT, "\tjmp .emit\n");
    EMIT(SECTION_TEXT, ".convert:\n");
    EMIT(SECTION_TEXT, "\txor ecx, ecx\n");
    EMIT(SECTION_TEXT, "\tcmp eax, 0\n");
    EMIT(SECTION_TEXT, "\tjge .abs_ready\n");
    EMIT(SECTION_TEXT, "\tneg eax\n");
    EMIT(SECTION_TEXT, "\tmov cl, 1\n");
    EMIT(SECTION_TEXT, ".abs_ready:\n");
    EMIT(SECTION_TEXT, "\tlea rsi, [rbp-1]\n");
    EMIT(SECTION_TEXT, ".convert_loop:\n");
    EMIT(SECTION_TEXT, "\txor edx, edx\n");
    EMIT(SECTION_TEXT, "\tmov ebx, 10\n");
    EMIT(SECTION_TEXT, "\tdiv ebx\n");
    EMIT(SECTION_TEXT, "\tadd dl, '0'\n");
    EMIT(SECTION_TEXT, "\tdec rsi\n");
    EMIT(SECTION_TEXT, "\tmov BYTE [rsi], dl\n");
    EMIT(SECTION_TEXT, "\ttest eax, eax\n");
    EMIT(SECTION_TEXT, "\tjne .convert_loop\n");
    EMIT(SECTION_TEXT, "\ttest cl, cl\n");
    EMIT(SECTION_TEXT, "\tjz .emit\n");
    EMIT(SECTION_TEXT, "\tdec rsi\n");
    EMIT(SECTION_TEXT, "\tmov BYTE [rsi], '-'\n");
    EMIT(SECTION_TEXT, ".emit:\n");
    EMIT(SECTION_TEXT, "\tlea rdx, [rbp-1]\n");
    EMIT(SECTION_TEXT, "\tsub rdx, rsi\n");
    EMIT(SECTION_TEXT, "\tmov eax, 1\n");
    EMIT(SECTION_TEXT, "\tmov edi, 1\n");
    EMIT(SECTION_TEXT, "\tmov rsi, rsi\n");
    EMIT(SECTION_TEXT, "\tsyscall\n");
    EMIT(SECTION_TEXT, "\tadd rsp, 48\n");
    EMIT(SECTION_TEXT, "\tpop rbp\n");
    EMIT(SECTION_TEXT, "\tret\n");
}

/* Emits a simple comment line. */
void x86_comment(char* text)
{
    EMIT(SECTION_TEXT, "; %s\n", text);
}

void x86_syscall(int code)
{
    EMIT(SECTION_TEXT, "\tmov %s, %d\n", RAX, code);
    EMIT(SECTION_TEXT, "\tsyscall\n");
}

char* x86_binop(ast* node)
{
    ENTER(BINOP);

    ast_binop* binop = &node->data.binop;

    // Reserve a register for the output
    char* out_reg = register_lock();
    char* lhs = x86_expr(binop->lhs);
    char* rhs = x86_expr(binop->rhs);

    // Emit the corresponding op
    switch (binop->op)
    {
    case BIN_ADD:
    {
        // Compute out_reg = lhs + rhs (preserve left-to-right order)
        EMIT(SECTION_TEXT, "\tmov %s, %s\n", out_reg, lhs);
        EMIT(SECTION_TEXT, "\tadd %s, %s\n", out_reg, rhs);
        // Release rhs
        register_unlock();
        // Release lhs
        register_unlock();

        break;
    }
    case BIN_SUB:
    {
        // Compute out_reg = lhs - rhs (preserve left-to-right order)
        EMIT(SECTION_TEXT, "\tmov %s, %s\n", out_reg, lhs);
        EMIT(SECTION_TEXT, "\tsub %s, %s\n", out_reg, rhs);
        // Release rhs
        register_unlock();
        // Release lhs
        register_unlock();

        break;
    }
    case BIN_MUL:
    {
        // Move lhs into the output register, then multiply by rhs
        EMIT(SECTION_TEXT, "\tmov %s, %s\n", out_reg, lhs);
        EMIT(SECTION_TEXT, "\timul %s, %s\n", out_reg, rhs);
        // Release rhs
        register_unlock();
        // Release lhs
        register_unlock();

        break;
    }
    case BIN_DIV:
    case BIN_EQ:
        ASSERT(false, "BINOP %s not implemented yet.",
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
    EMIT(SECTION_DATA, "\t%s: dq %d\n", name, 0);
    EXIT(DECLVAR);
}

void x86_block(ast* node)
{
    ASSERT(node->type == AST_BLOCK, "Expected BLOCK node, got %s",
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

    EMIT(SECTION_GLOBAL, "global %s\n", name);
    EMIT(SECTION_TEXT, "%s:\n", name);

    x86_prologue();

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

    ASSERT(symbol != NULL, "Failed to resolve symbol for %s", name);

    if (symbol->type == SYMBOL_GLOBAL)
    {
        EMIT(SECTION_TEXT, "\tmov [%s], %s\n", name, rhs_reg);
    }
    else
    {
        // Stack locals are addressed relative to RBP.
        EMIT(SECTION_TEXT, "\tmov [rbp%+td], %s\n", symbol->offset, rhs_reg);
    }

    if (rhs->type != AST_CALL)
    {
        register_unlock();
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
        ASSERT(!register_get("rdi")->locked,
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
        ASSERT(false,
               "Invalid right-hand type for RETURN: %d. Wanted one of "
               "[BINOP, CONSTANT, IDENTIFIER, CALL].",
               rhs->type);
    }

    // If this is NOT the main function, just do a normal return.
    // This will return the value in RAX.
    if (!g_in_main)
    {
        // Move the result into RAX
        EMIT(SECTION_TEXT, "\tmov rax, %s\n", rhs_reg);
        if (rhs->type != AST_CALL)
        {
            register_unlock();
        }
        x86_epilogue(true);
    }
    // Otherwise we need to exit the program. Move RAX into RDI,
    // which is the first argument for the linux exit syscall.
    else
    {
        EMIT(SECTION_TEXT, "\tmov rdi, %s\n", rhs_reg);
        if (rhs->type != AST_CALL)
        {
            register_unlock();
        }
        x86_epilogue(false);
        x86_syscall(X86_EXIT);
    }
    EXIT(RET);
}

char* x86_call(ast* node)
{
    ENTER(CALL);
    char* reg = RAX;
    char* callee = node->data.call.identifier->data.identifier.name;
    size_t arg_count = node->data.call.count;

    // print(int value)
    if (strcmp(callee, BUILTIN_PRINT) == 0)
    {
        ASSERT(arg_count == 1, "`print` expects exactly one argument.");
        ast* arg = node->data.call.args[0];
        ASSERT(arg->type == AST_CONSTANT || arg->type == AST_IDENTIFIER,
               "`print` argument must be a constant or identifier.");

        char* arg_reg = x86_expr(arg);
        EMIT(SECTION_TEXT, "\tmov rdi, %s\n", arg_reg);
        if (arg->type != AST_CALL)
        {
            register_unlock();
        }
        EMIT(SECTION_TEXT, "\tcall _%s\n", BUILTIN_PRINT);
        EXIT(CALL);
        return reg;
    }

    EMIT(SECTION_TEXT, "\tcall %s\n", callee);
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
        reg = register_lock();
        // Move the constant into this register
        EMIT(SECTION_TEXT, "\tmov %s, %d\n", reg, node->data.constant.value);
        break;
    case AST_IDENTIFIER:
        // Get a new register to store the identifier's value
        reg = register_lock();
        {
            symbol_t* symbol = symbol_resolve(node->data.identifier.name);
            if (symbol->type == SYMBOL_GLOBAL)
            {
                EMIT(SECTION_TEXT, "\tmov %s, [%s]\n", reg, symbol->name);
            }
            else
            {
                // Load local values via their recorded stack offset.
                EMIT(SECTION_TEXT, "\tmov %s, [rbp%+td]\n", reg,
                     symbol->offset);
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
    case AST_CALL:
        x86_call(node);
        break;
    default:
        break;
    }
    EXIT(STMT);
}

void x86_body(ast* node)
{
    ASSERT(node->type == AST_BODY, "Wanted node type BODY, got %s", node->type);
    ENTER(BODY);
    for (size_t i = 0; i < node->data.body.count; i++)
    {
        ast* statement = node->data.body.statements[i];
        x86_statement(statement);
    }
    EXIT(BODY);
}

void x86_program(ast* node)
{
    ASSERT(node->type == AST_PROGRAM, "Wanted node type PROGRAM, got %s",
           node->type);
    ENTER(PROGRAM);

    // Initialize scope state
    scope_free(g_global_scope);
    g_global_scope = scope_new(NULL);
    g_scope = g_global_scope;
    g_in_function = false;
    g_stack_offset = 0;

    // Collect all global symbols prior to emitting any code.
    get_global_symbols(node);

    // Make all symbol references RIP-relative by default
    // https://www.nasm.us/doc/nasm08.html#section-8.2.1
    EMIT(SECTION_GLOBAL, "default rel\n");

    // Initialize sections
    EMIT(SECTION_BSS, "section .bss\n");
    EMIT(SECTION_DATA, "section .data\n");
    EMIT(SECTION_TEXT, "section .text\n");

    x86_print();

    for (size_t i = 0; i < node->data.program.count; i++)
    {
        ast* body = node->data.program.body[i];
        x86_body(body);
    }

    scope_free(g_global_scope);
    g_global_scope = NULL;
    g_scope = NULL;
    EXIT(PROGRAM);
}