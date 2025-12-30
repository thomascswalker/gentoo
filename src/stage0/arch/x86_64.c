#include <stddef.h>
#include <string.h>

#include "ast.h"
#include "buffer.h"
#include "codegen.h"
#include "log.h"
#include "macros.h"
#include "reg.h"
#include "stdlib.h"
#include "x86_64.h"

#define FN_CONCAT "concat"
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
            .string = x86_string,
            .expr = x86_expr,
            .syscall = x86_syscall,
            .comment = x86_comment,
            .epilogue = x86_epilogue,
        },
    .type = X86_64,
};

bool g_in_function = false;
scope_t* g_scope = NULL;
scope_t* g_global_scope = NULL;
ptrdiff_t g_stack_offset = 0;
int g_string_count = 0;
bool g_concat_helper_emitted = false;

static const char* ARG_REGISTERS[] = {RDI, RSI, RDX, RCX, R8, R9};
static const size_t ARG_REGISTER_COUNT =
    sizeof(ARG_REGISTERS) / sizeof(ARG_REGISTERS[0]);

static void emit_concat(void);

// Concatenates two strings by allocating a new buffer for the resultant string.
static char* x86_concat_strings(ast* lhs_node, ast* rhs_node)
{
    ENTER(STR_CONCAT);

    // Evaluate both operands so we have registers holding their addresses.
    char* lhs_reg = x86_expr(lhs_node);
    char* rhs_reg = x86_expr(rhs_node);

    // Move the evaluated pointers into calling-convention registers.
    EMIT(SECTION_TEXT, "\tmov rdi, %s\n", lhs_reg);
    EMIT(SECTION_TEXT, "\tmov rsi, %s\n", rhs_reg);

    if (rhs_node->type != AST_CALL)
    {
        register_unlock();
    }
    if (lhs_node->type != AST_CALL)
    {
        register_unlock();
    }

    // Call the shared helper which returns the concatenated buffer in RAX.
    EMIT(SECTION_TEXT, "\tcall %s\n", FN_CONCAT);

    char* dest_reg = register_lock();
    EMIT(SECTION_TEXT, "\tmov %s, rax\n", dest_reg);

    EXIT(STR_CONCAT);
    return dest_reg;
}

static void emit_concat(void)
{
    EMIT(SECTION_TEXT, "%s:\n", FN_CONCAT);
    EMIT(SECTION_TEXT, "\tpush rbp\n");
    EMIT(SECTION_TEXT, "\tmov rbp, rsp\n");
    EMIT(SECTION_TEXT, "\tsub rsp, 40\n");
    EMIT(SECTION_TEXT, "\tmov [rbp-8], rdi\n");
    EMIT(SECTION_TEXT, "\tmov [rbp-16], rsi\n");
    EMIT(SECTION_TEXT, "\tmov rdi, [rbp-8]\n");
    EMIT(SECTION_TEXT, "\tcall strlen\n");
    EMIT(SECTION_TEXT, "\tmov [rbp-24], rax\n");
    EMIT(SECTION_TEXT, "\tmov rdi, [rbp-16]\n");
    EMIT(SECTION_TEXT, "\tcall strlen\n");
    EMIT(SECTION_TEXT, "\tmov [rbp-32], rax\n");
    EMIT(SECTION_TEXT, "\tmov rax, [rbp-24]\n");
    EMIT(SECTION_TEXT, "\tadd rax, [rbp-32]\n");
    EMIT(SECTION_TEXT, "\tadd rax, 1\n");
    EMIT(SECTION_TEXT, "\tmov rdi, rax\n");
    EMIT(SECTION_TEXT, "\tcall malloc\n");
    EMIT(SECTION_TEXT, "\tmov [rbp-40], rax\n");
    EMIT(SECTION_TEXT, "\tmov rdi, rax\n");
    EMIT(SECTION_TEXT, "\tmov rsi, [rbp-8]\n");
    EMIT(SECTION_TEXT, "\tcall strcpy\n");
    EMIT(SECTION_TEXT, "\tmov rdi, [rbp-40]\n");
    EMIT(SECTION_TEXT, "\tmov rsi, [rbp-16]\n");
    EMIT(SECTION_TEXT, "\tcall strcat\n");
    EMIT(SECTION_TEXT, "\tmov rax, [rbp-40]\n");
    EMIT(SECTION_TEXT, "\tadd rsp, 40\n");
    EMIT(SECTION_TEXT, "\tpop rbp\n");
    EMIT(SECTION_TEXT, "\tret\n");
}

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
    symbol->value_kind = SYMBOL_VALUE_UNKNOWN;
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

static symbol_value_t get_symbol_value_kind(ast* node)
{
    if (!node)
    {
        return SYMBOL_VALUE_UNKNOWN;
    }

    switch (node->type)
    {
    case AST_CONSTANT:
        if (node->data.constant.type == TYPE_STRING)
        {
            return SYMBOL_VALUE_STRING;
        }
        return SYMBOL_VALUE_INT;
    case AST_STRING:
        return SYMBOL_VALUE_STRING;
    case AST_IDENTIFIER:
    {
        symbol_t* symbol = symbol_resolve(node->data.identifier.name);
        ASSERT(symbol->value_kind != SYMBOL_VALUE_UNKNOWN,
               "Symbol '%s' has unknown type.", symbol->name);
        return symbol->value_kind;
    }
    case AST_BINOP:
    {
        symbol_value_t lhs = get_symbol_value_kind(node->data.binop.lhs);
        symbol_value_t rhs = get_symbol_value_kind(node->data.binop.rhs);

        ASSERT(lhs != SYMBOL_VALUE_UNKNOWN,
               "Left-hand symbol has unknown type.");
        ASSERT(rhs != SYMBOL_VALUE_UNKNOWN,
               "Right-hand symbol has unknown type.");

        log_debug("lhs: %s", ast_to_string(node->data.binop.lhs->type));
        log_debug("rhs: %s", ast_to_string(node->data.binop.rhs->type));

        switch (node->data.binop.op)
        {
        case BIN_ADD:
            if (lhs == SYMBOL_VALUE_STRING && rhs == SYMBOL_VALUE_STRING)
            {
                return SYMBOL_VALUE_STRING;
            }
            ASSERT(lhs == SYMBOL_VALUE_INT && rhs == SYMBOL_VALUE_INT,
                   "Cannot add %s to %s.", symbol_value_to_string(lhs),
                   symbol_value_to_string(rhs));
            return SYMBOL_VALUE_INT;
        case BIN_SUB:
        case BIN_MUL:
        case BIN_DIV:
        case BIN_EQ:
            ASSERT(lhs == SYMBOL_VALUE_INT && rhs == SYMBOL_VALUE_INT,
                   "Operator %s only supports integers.",
                   binop_to_string(node->data.binop.op));
            return SYMBOL_VALUE_INT;
        default:
            break;
        }
        break;
    }
    case AST_CALL:
        return SYMBOL_VALUE_INT;
    default:
        break;
    }

    return SYMBOL_VALUE_INT;
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
                symbol_t* symbol = scope_lookup_shallow(g_global_scope, name);
                if (symbol == NULL)
                {
                    symbol =
                        scope_add_symbol(g_global_scope, name, SYMBOL_GLOBAL);
                }

                symbol_value_t rhs_kind =
                    get_symbol_value_kind(statement->data.assign.rhs);
                if (symbol->value_kind == SYMBOL_VALUE_UNKNOWN)
                {
                    symbol->value_kind = rhs_kind;
                }
                else
                {
                    ASSERT(symbol->value_kind == rhs_kind,
                           "Global '%s' type mismatch (%s vs %s).", name,
                           symbol_value_to_string(symbol->value_kind),
                           symbol_value_to_string(rhs_kind));
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

    if (binop->op == BIN_ADD)
    {
        symbol_value_t lhs_kind = get_symbol_value_kind(binop->lhs);
        symbol_value_t rhs_kind = get_symbol_value_kind(binop->rhs);
        if (lhs_kind == SYMBOL_VALUE_STRING && rhs_kind == SYMBOL_VALUE_STRING)
        {
            char* string_reg = x86_concat_strings(binop->lhs, binop->rhs);
            EXIT(BINOP);
            return string_reg;
        }
    }

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

    g_in_function = true;
    g_stack_offset = 0;

    EMIT(SECTION_GLOBAL, "global %s\n", name);
    EMIT(SECTION_TEXT, "%s:\n", name);

    x86_prologue();

    x86_block(node->data.declfn.block);

    g_in_function = prev_in_function;
    g_stack_offset = prev_stack_offset;
    EXIT(DECLFN);
}

void x86_assign(ast* node)
{
    ENTER(ASSIGN);

    // Emit the right hand side first (fully processing any expressions)
    ast* rhs = node->data.assign.rhs;
    symbol_value_t rhs_kind = get_symbol_value_kind(rhs);
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

    if (symbol->value_kind == SYMBOL_VALUE_UNKNOWN)
    {
        symbol->value_kind = rhs_kind;
    }
    else
    {
        ASSERT(symbol->value_kind == rhs_kind,
               "Cannot assign %s value to %s (expected %s).",
               symbol_value_to_string(rhs_kind), name,
               symbol_value_to_string(symbol->value_kind));
    }

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
    ast* rhs = node->data.ret.node;
    char* rhs_reg;

    switch (rhs->type)
    {
    case AST_BINOP:
    case AST_CONSTANT:
    case AST_STRING:
    case AST_IDENTIFIER:
    case AST_CALL:
        rhs_reg = x86_expr(rhs);
        break;
    default:
        ASSERT(false,
               "Invalid right-hand type for RETURN: %d. Wanted one of "
               "[BINOP, CONSTANT, STRING, IDENTIFIER, CALL].",
               rhs->type);
    }

    // Move the result into RAX before returning to the caller.
    EMIT(SECTION_TEXT, "\tmov rax, %s\n", rhs_reg);
    if (rhs->type != AST_CALL)
    {
        register_unlock();
    }
    x86_epilogue(true);
    EXIT(RET);
}

char* x86_call(ast* node)
{
    ENTER(CALL);
    char* reg = RAX;
    char* callee = node->data.call.identifier->data.identifier.name;
    size_t arg_count = node->data.call.count;
    size_t reg_arg_count =
        arg_count < ARG_REGISTER_COUNT ? arg_count : ARG_REGISTER_COUNT;
    size_t stack_arg_count =
        arg_count > ARG_REGISTER_COUNT ? arg_count - ARG_REGISTER_COUNT : 0;

    // Push stack arguments (evaluated right-to-left) so they land on the stack
    // in the expected order for the System V ABI.
    for (size_t idx = arg_count; idx > reg_arg_count;)
    {
        idx--;
        ast* arg = node->data.call.args[idx];
        char* arg_reg = x86_expr(arg);
        EMIT(SECTION_TEXT, "\tpush %s\n", arg_reg);
        if (arg->type != AST_CALL)
        {
            register_unlock();
        }
    }

    // Evaluate register arguments left-to-right, push to preserve, then pop
    // them into the actual calling-convention registers in reverse order.
    for (size_t i = 0; i < reg_arg_count; i++)
    {
        ast* arg = node->data.call.args[i];
        char* arg_reg = x86_expr(arg);
        EMIT(SECTION_TEXT, "\tpush %s\n", arg_reg);
        if (arg->type != AST_CALL)
        {
            register_unlock();
        }
    }

    for (size_t i = reg_arg_count; i > 0; i--)
    {
        const char* target = ARG_REGISTERS[i - 1];
        EMIT(SECTION_TEXT, "\tpop %s\n", target);
    }

    // System V varargs require RAX to contain the number of vector registers
    // used. We only pass integer arguments, so set it to zero.
    EMIT(SECTION_TEXT, "\txor rax, rax\n");
    EMIT(SECTION_TEXT, "\tcall %s\n", callee);

    if (stack_arg_count > 0)
    {
        EMIT(SECTION_TEXT, "\tadd rsp, %zu\n", stack_arg_count * 8);
    }

    EXIT(CALL);
    return reg;
}

char* x86_string(char* text)
{
    // Create a new buffer for the line we're going to format. This is to
    // manage memory in a more efficient way.
    buffer_t* line = buffer_new();

    // Define the name as 'printf_string_n' where 'n' is the current
    // string count.
    // Always define as bytes.
    char* string_name = formats("printf_string_%d", g_string_count);
    buffer_printf(line, "\t%s: db ", string_name);

    // Write each character of the string individually in order
    // to emit the exact string as an array of bytes.
    //
    // For example, having "\n" is recognized as '\' and 'n', a two character
    // string, rather than the explicit '\n' value. While this is 'messier'
    // for the output assembly, it maintains a 1:1 mapping of the input
    // string and the output string.
    //
    // "dog" => 0x64, 0x6F, 0x67
    size_t text_len = strlen(text);
    for (size_t i = 0; i < text_len; i++)
    {
        // Only pre-pend ", " if we're on the second or greater
        // character.
        if (i > 0)
        {
            buffer_puts(line, ", ");
        }

        // Output the current character as a hexadecimal integer.
        buffer_printf(line, "0x%02X", (uint8_t)text[i]);
    }

    // Always end with a null-terminator
    buffer_puts(line, ", 0\n");

    EMIT(SECTION_DATA, line->data);

    buffer_free(line);
    g_string_count++;

    // Return the name of the string's symbol
    return string_name;
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
    case AST_STRING:
    {
        reg = register_lock();
        // Declare a new string
        char* string_label = x86_string(node->data.string.value);
        // Store the result of the string into the register
        EMIT(SECTION_TEXT, "\tlea %s, [%s]\n", reg, string_label);
        break;
    }
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

    emit_concat();

    // External built-ins
    EMIT(SECTION_GLOBAL, "extern printf\n");
    EMIT(SECTION_GLOBAL, "extern malloc\n");
    EMIT(SECTION_GLOBAL, "extern free\n");
    EMIT(SECTION_GLOBAL, "extern memcpy\n");
    EMIT(SECTION_GLOBAL, "extern strlen\n");
    EMIT(SECTION_GLOBAL, "extern strcat\n");
    EMIT(SECTION_GLOBAL, "extern strcpy\n");

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