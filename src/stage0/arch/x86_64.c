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

static codegen_context_t g_ctx = {
    .current_scope = NULL,
    .global_scope = NULL,
    .stack_offset = 0,
    .string_count = 0,
    .branch_count = 0,
    .in_function = false,
    .current_function_name = NULL,
    .expected_return_type = SYMBOL_VALUE_UNKNOWN,
};

/* x86 registers used for passing arguments */

static const char* ARG_REGISTERS[] = {RDI, RSI, RDX, RCX, R8, R9};
static const size_t ARG_REGISTER_COUNT =
    sizeof(ARG_REGISTERS) / sizeof(ARG_REGISTERS[0]);

// Concatenates two runtime strings via the shared helper, returning a register
// that holds the newly allocated buffer address.
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

// Emits the helper function that performs heap-allocated string concatenation
// so user code can rely on one implementation.
static void emit_concat(void)
{
    EMIT(SECTION_TEXT, "%s:\n", FN_CONCAT);
    // Function prologue and a small spill area for temporaries/locals.
    EMIT(SECTION_TEXT, "\tpush rbp\n");
    EMIT(SECTION_TEXT, "\tmov rbp, rsp\n");
    EMIT(SECTION_TEXT, "\tsub rsp, 40\n");
    // Persist the incoming string pointers on the stack frame.
    EMIT(SECTION_TEXT, "\tmov [rbp-8], rdi\n");
    EMIT(SECTION_TEXT, "\tmov [rbp-16], rsi\n");
    // Measure lhs length and stash the result.
    EMIT(SECTION_TEXT, "\tmov rdi, [rbp-8]\n");
    EMIT(SECTION_TEXT, "\tcall strlen\n");
    EMIT(SECTION_TEXT, "\tmov [rbp-24], rax\n");
    // Measure rhs length and stash the result.
    EMIT(SECTION_TEXT, "\tmov rdi, [rbp-16]\n");
    EMIT(SECTION_TEXT, "\tcall strlen\n");
    EMIT(SECTION_TEXT, "\tmov [rbp-32], rax\n");
    // Compute total size (lhs + rhs + null terminator) and allocate buffer.
    EMIT(SECTION_TEXT, "\tmov rax, [rbp-24]\n");
    EMIT(SECTION_TEXT, "\tadd rax, [rbp-32]\n");
    EMIT(SECTION_TEXT, "\tadd rax, 1\n");
    EMIT(SECTION_TEXT, "\tmov rdi, rax\n");
    EMIT(SECTION_TEXT, "\tcall malloc\n");
    EMIT(SECTION_TEXT, "\tmov [rbp-40], rax\n");
    // Copy lhs into the destination buffer.
    EMIT(SECTION_TEXT, "\tmov rdi, rax\n");
    EMIT(SECTION_TEXT, "\tmov rsi, [rbp-8]\n");
    EMIT(SECTION_TEXT, "\tcall strcpy\n");
    // Append rhs immediately after lhs in the buffer.
    EMIT(SECTION_TEXT, "\tmov rdi, [rbp-40]\n");
    EMIT(SECTION_TEXT, "\tmov rsi, [rbp-16]\n");
    EMIT(SECTION_TEXT, "\tcall strcat\n");
    // Move the result pointer into RAX and tear down the frame.
    EMIT(SECTION_TEXT, "\tmov rax, [rbp-40]\n");
    EMIT(SECTION_TEXT, "\tadd rsp, 40\n");
    EMIT(SECTION_TEXT, "\tpop rbp\n");
    EMIT(SECTION_TEXT, "\tret\n");
}

/* Scope & Symbols */

// Allocates a new scope chained to the provided parent, pre-sizing symbol
// storage so we can push locals efficiently while recursing through blocks.
scope_t* scope_new(scope_t* parent)
{
    scope_t* scope = (scope_t*)calloc(1, sizeof(scope_t));
    // Chain to parent so lookups can walk outward when resolving symbols.
    scope->parent = parent;
    // Start with a modest capacity to minimize allocations for small blocks.
    scope->capacity = 8;
    scope->symbols = (symbol_t*)calloc(scope->capacity, sizeof(symbol_t));
    return scope;
}

// Releases the memory associated with a scope; used when unwinding blocks.
void scope_free(scope_t* scope)
{
    if (!scope)
    {
        return;
    }
    // Symbols are POD so freeing the backing array cleans up the entries.
    free(scope->symbols);
    free(scope);
}

// Creates a child scope of the current scope, establishing isolation for
// nested blocks.
void scope_push()
{
    ASSERT(g_ctx.current_scope != NULL, "Cannot push scope with no parent.");
    // Create a child scope whose parent is the current scope and make it
    // active. Locals declared after this point live inside this new scope.
    g_ctx.current_scope = scope_new(g_ctx.current_scope);
}

// Pops the current scope and returns control to its parent, freeing all local
// symbol metadata allocated for that lexical block.
void scope_pop()
{
    ASSERT(g_ctx.current_scope != NULL, "No scope to pop.");
    scope_t* current = g_ctx.current_scope;
    ASSERT(current->parent != NULL, "Cannot pop the global scope.");
    // Restore the parent scope and release the storage used for the child.
    g_ctx.current_scope = current->parent;
    scope_free(current);
}

// Finds a symbol only within the provided scope; callers use this to avoid
// shadowing collisions when declaring new bindings.
symbol_t* scope_lookup_shallow(scope_t* scope, const char* name)
{
    if (!scope)
    {
        return NULL;
    }
    // Search only the provided scope so we can enforce one definition per
    // block when adding locals.
    for (size_t i = 0; i < scope->count; i++)
    {
        if (strcmp(scope->symbols[i].name, name) == 0)
        {
            return &scope->symbols[i];
        }
    }
    return NULL;
}

// Walks parent scopes outward until it finds a symbol by name, mirroring how
// the runtime would resolve identifiers.
symbol_t* scope_lookup(scope_t* scope, const char* name)
{
    scope_t* current = scope;
    while (current)
    {
        // Try the innermost scope first; if not found, walk to the parent and
        // keep searching until a match is discovered or we hit the root.
        symbol_t* symbol = scope_lookup_shallow(current, name);
        if (symbol)
        {
            return symbol;
        }
        current = current->parent;
    }
    return NULL;
}

// Adds a symbol of the given scope type, expanding storage as necessary and
// logging the result for easier debugging of symbol lifetimes.
symbol_t* scope_add_symbol(scope_t* scope, const char* name,
                           symbol_scope_t type)
{
    ASSERT(scope != NULL, "Scope cannot be NULL when adding a symbol.");

    if (scope->count >= scope->capacity)
    {
        // Grow the underlying array and zero the newly added slots so future
        // bookkeeping starts from a clean state.
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
    symbol->value_type = SYMBOL_VALUE_UNKNOWN;
    symbol->offset = 0;

    char* message = symbol_to_string(symbol);
    log_debug("New symbol: %s", message);
    free(message);
    return symbol;
}

// Reserves eight bytes on the stack for a local value and returns its offset
// relative to RBP so loads and stores can reference it.
ptrdiff_t allocate_stack_slot()
{
    ASSERT(g_ctx.in_function,
           "Stack slots can only be allocated inside functions.");
    // Move the stack pointer once per slot and track the total offset so we
    // can address locals relative to RBP.
    g_ctx.stack_offset += 8;
    EMIT(SECTION_TEXT, "\tsub rsp, 8\n");
    return -g_ctx.stack_offset;
}

// Declares a new global symbol and asserts no duplicate exists at the module
// scope.
symbol_t* symbol_define_global(const char* name)
{
    ASSERT(g_ctx.global_scope != NULL, "Global scope is not initialized.");
    symbol_t* existing = scope_lookup_shallow(g_ctx.global_scope, name);
    ASSERT(existing == NULL, "Global symbol %s already defined.", name);
    // Record the new binding in the global scope table so it can be referenced
    // from anywhere in the program.
    return scope_add_symbol(g_ctx.global_scope, name, SYMBOL_GLOBAL);
}

// Creates a local symbol inside the current function scope and hands back the
// reserved stack slot.
symbol_t* symbol_define_local(const char* name)
{
    ASSERT(g_ctx.current_scope != NULL, "Current scope is not set.");
    ASSERT(g_ctx.current_scope != g_ctx.global_scope,
           "Local declarations require a function scope.");
    symbol_t* existing = scope_lookup_shallow(g_ctx.current_scope, name);
    ASSERT(existing == NULL, "Symbol %s already defined in this scope.", name);

    symbol_t* symbol =
        scope_add_symbol(g_ctx.current_scope, name, SYMBOL_LOCAL);
    // Locals reside on the stack, so reserve and record their frame offset.
    symbol->offset = allocate_stack_slot();
    return symbol;
}

// Resolves an identifier to a previously declared symbol, enforcing that any
// use must have been defined earlier in scope search order.
symbol_t* symbol_resolve(const char* name)
{
    // Walk outward through scopes (starting from current) until a declaration
    // appears. This enforces Gentoo's requirement that identifiers must be
    // defined in an enclosing lexical scope.
    symbol_t* symbol = scope_lookup(g_ctx.current_scope, name);
    ASSERT(symbol != NULL, "Undefined symbol: %s", name);
    return symbol;
}

// Infers the static value type represented by the AST node so type checks can
// enforce valid operations (e.g., string concatenation) before emitting code.
symbol_value_t get_symbol_value_type(ast* node)
{
    if (!node)
    {
        return SYMBOL_VALUE_UNKNOWN;
    }

    // Determine the value type by looking at the syntactic shape; literals and
    // explicit type annotations provide their type immediately, while
    // identifiers require symbol resolution.
    switch (node->type)
    {
    case AST_TYPE:
    {
        switch (node->data.type.type)
        {
        case TYPE_VOID:
        {
            return SYMBOL_VALUE_VOID;
        }
        case TYPE_BOOL:
        {
            return SYMBOL_VALUE_BOOL;
        }
        case TYPE_INT:
        {
            return SYMBOL_VALUE_INT;
        }
        case TYPE_STRING:
        {
            return SYMBOL_VALUE_STRING;
        }
        default:
        {
            return SYMBOL_VALUE_INT;
        }
        }
    }
    // Constants can only be one of BOOL, INT, or STRING
    case AST_CONSTANT:
    {
        switch (node->data.constant.type)
        {
        case TYPE_BOOL:
        {
            return SYMBOL_VALUE_BOOL;
        }
        case TYPE_STRING:
        {
            return SYMBOL_VALUE_STRING;
        }
        default:
        {
            return SYMBOL_VALUE_INT;
        }
        }
    }
    // Strings can only be STRING
    case AST_STRING:
    {
        return SYMBOL_VALUE_STRING;
    }
    // Resolve the symbol of the identifier by its name and return the symbol's
    // `value_type`.
    case AST_IDENTIFIER:
    {
        char* name = node->data.identifier.name;
        symbol_t* symbol = symbol_resolve(name);
        ASSERT(symbol->value_type != SYMBOL_VALUE_UNKNOWN,
               "Symbol '%s' has unknown type.", symbol->name);
        return symbol->value_type;
    }
    // Evalutate the binary operation and determine the resulting symbol value
    // type.
    case AST_BINOP:
    {
        symbol_value_t lhs = get_symbol_value_type(node->data.binop.lhs);
        symbol_value_t rhs = get_symbol_value_type(node->data.binop.rhs);

        ASSERT(lhs != SYMBOL_VALUE_UNKNOWN,
               "Left-hand symbol has unknown type.");
        ASSERT(rhs != SYMBOL_VALUE_UNKNOWN,
               "Right-hand symbol has unknown type.");

        log_debug("lhs: %s", ast_to_string(node->data.binop.lhs->type));
        log_debug("rhs: %s", ast_to_string(node->data.binop.rhs->type));

        switch (node->data.binop.op)
        {
        case BIN_ADD:
        { // Strings can only be added to strings
            if (lhs == SYMBOL_VALUE_STRING && rhs == SYMBOL_VALUE_STRING)
            {
                return SYMBOL_VALUE_STRING;
            }

            // Otherwise only allow adding ints to ints.
            ASSERT(lhs == SYMBOL_VALUE_INT && rhs == SYMBOL_VALUE_INT,
                   "Cannot add %s to %s.", symbol_value_to_string(lhs),
                   symbol_value_to_string(rhs));
            return SYMBOL_VALUE_INT;
        }
        case BIN_SUB:
        case BIN_MUL:
        case BIN_DIV:
        { // Only allow subtracting, multiplying, and dividing ints by ints.
            ASSERT(lhs == SYMBOL_VALUE_INT && rhs == SYMBOL_VALUE_INT,
                   "Operator %s only supports integers.",
                   binop_to_string(node->data.binop.op));
            return SYMBOL_VALUE_INT;
        }
        case BIN_EQ:
        {
            ASSERT(lhs == rhs, "Equality only supports comparing same types.");
            return SYMBOL_VALUE_BOOL;
        }
        case BIN_GT:
        case BIN_LT:
        {
            ASSERT(lhs == SYMBOL_VALUE_INT && rhs == SYMBOL_VALUE_INT,
                   "Operator %s only supports integers.",
                   binop_to_string(node->data.binop.op));
            return SYMBOL_VALUE_BOOL;
        }
        default:
        {
            break;
        }
        }
        break;
    }
    // Return the function's return type
    case AST_CALL:
    {
        symbol_t* symbol =
            symbol_resolve(node->data.call.identifier->data.identifier.name);
        return symbol->ret_type;
    }
    default:
    {
        break;
    }
    }

    return SYMBOL_VALUE_UNKNOWN;
}

// Scans the program before emission to discover globals and infer their types
// so subsequent references know each symbol's storage class.
void x86_globals(ast* node)
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
                // Only declarations at the top level become globals; record
                // them so codegen knows every symbol up front.
                char* name = lhs->data.declvar.identifier->data.identifier.name;
                symbol_t* symbol =
                    scope_lookup_shallow(g_ctx.global_scope, name);
                if (symbol == NULL)
                {
                    symbol = scope_add_symbol(g_ctx.global_scope, name,
                                              SYMBOL_GLOBAL);
                }

                symbol_value_t rhs_type =
                    get_symbol_value_type(statement->data.assign.rhs);
                // The first assignment sets the type, subsequent ones must
                // match to avoid conflicting global definitions.
                if (symbol->value_type == SYMBOL_VALUE_UNKNOWN)
                {
                    symbol->value_type = rhs_type;
                }
                else
                {
                    ASSERT(symbol->value_type == rhs_type,
                           "Global '%s' type mismatch (%s vs %s).", name,
                           symbol_value_to_string(symbol->value_type),
                           symbol_value_to_string(rhs_type));
                }
            }
        }
    }
}

/* Emitters */

// Restores the caller's stack frame and optionally emits a `ret`, shared by
// normal returns and synthesized epilogues.
void x86_epilogue(bool returns)
{
    // Tear down this stack frame so the caller regains ownership of RSP/RBP.
    EMIT(SECTION_TEXT, "\tmov rsp, rbp\n");
    EMIT(SECTION_TEXT, "\tpop rbp\n");
    if (returns)
    {
        // Only emit `ret` when ending a function, not internal helper
        // epilogues.
        EMIT(SECTION_TEXT, "\tret\n");
    }
}

// Establishes the standard System V stack frame for a function entry.
void x86_prologue()
{
    // Save the caller's RBP and anchor a fresh base pointer at the current SP.
    EMIT(SECTION_TEXT, "\tpush rbp\n");
    EMIT(SECTION_TEXT, "\tmov rbp, rsp\n");
}

// Emits a simple comment line.
void x86_comment(char* text)
{
    // Comments are prefixed with ';' in NASM syntax.
    EMIT(SECTION_TEXT, "; %s\n", text);
}

// Emits a Linux syscall invocation with the provided code in RAX.
void x86_syscall(int code)
{
    // System V ABI expects the syscall number in RAX before invoking `syscall`.
    EMIT(SECTION_TEXT, "\tmov %s, %d\n", RAX, code);
    EMIT(SECTION_TEXT, "\tsyscall\n");
}

// Low-level emitter for arithmetic and comparison expressions; evaluates both
// operands, enforces type rules, then writes the machine operations.
char* x86_binop(ast* node)
{
    ENTER(BINOP);

    ast_binop* binop = &node->data.binop;

    if (binop->op == BIN_ADD)
    {
        symbol_value_t lhs_type = get_symbol_value_type(binop->lhs);
        symbol_value_t rhs_type = get_symbol_value_type(binop->rhs);
        if (lhs_type == SYMBOL_VALUE_STRING && rhs_type == SYMBOL_VALUE_STRING)
        {
            // String concatenation is implemented via the helper; bail out of
            // the numeric pipeline once we detect both operands are strings.
            char* string_reg = x86_concat_strings(binop->lhs, binop->rhs);
            EXIT(BINOP);
            return string_reg;
        }
    }

    // Reserve a register to hold the result of the operation, then evaluate
    // the operands so their values reside in registers before we emit ops.
    char* out_reg = register_lock();
    char* lhs = x86_expr(binop->lhs);
    char* rhs = x86_expr(binop->rhs);

    // Emit the instruction sequence matching the requested operator.
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
        ASSERT(false, "BINOP %s not implemented yet.",
               binop_to_string(binop->op));
        break;
    case BIN_EQ:
    case BIN_GT:
    case BIN_LT:
    {
        // All comparison forms reuse the same register pattern: compare the
        // operands, load 0/1 sentinels, then conditionally move the truthy
        // value into the output register.
        const char* condition = NULL;
        if (binop->op == BIN_EQ)
        {
            condition = "cmove";
        }
        else if (binop->op == BIN_GT)
        {
            condition = "cmovg";
        }
        else
        {
            condition = "cmovl";
        }

        EMIT(SECTION_TEXT, "\tcmp %s, %s\n", lhs, rhs);
        // Release rhs
        register_unlock();
        // Release lhs
        register_unlock();

        EMIT(SECTION_TEXT, "\tmov %s, 0\n", out_reg);
        char* true_reg = register_lock();
        ASSERT(true_reg != NULL,
               "Unable to allocate register for comparison result.");
        EMIT(SECTION_TEXT, "\tmov %s, 1\n", true_reg);
        EMIT(SECTION_TEXT, "\t%s %s, %s\n", condition, out_reg, true_reg);
        register_unlock();
        break;
    }
    default:
        break;
    }
    EXIT(BINOP);
    return out_reg;
}

// Emits storage for a global variable declaration in the data section.
void x86_declvar(ast* node)
{
    ENTER(DECLVAR);
    char* name = node->data.declvar.identifier->data.identifier.name;
    // Reserve eight bytes (dq) initialized to zero for this global symbol.
    EMIT(SECTION_DATA, "\t%s: dq %d\n", name, 0);
    EXIT(DECLVAR);
}

// Executes each statement in a lexical block while creating a nested scope for
// locals introduced inside the block.
void x86_block(ast* node)
{
    ASSERT(node->type == AST_BLOCK, "Expected BLOCK node, got %s",
           ast_to_string(node->type));

    // Each block introduces a fresh scope to keep locals isolated.
    scope_push();
    ast_block* block = &node->data.block;
    for (int i = 0; i < block->count; i++)
    {
        // Emit each statement in order; side-effects accumulate on the current
        // scope and stack frame until the block completes.
        x86_statement(block->statements[i]);
    }
    scope_pop();
}

// Emits a function: records its symbol, sets up a new frame, runs the body,
// and restores the previous emission state afterward.
void x86_declfn(ast* node)
{
    ENTER(DECLFN);

    // Get the function name
    char* name = node->data.declfn.identifier->data.identifier.name;

    // Define a new global symbol if it's not found
    symbol_t* symbol = scope_lookup_shallow(g_ctx.global_scope, name);
    if (!symbol)
    {
        symbol = symbol_define_global(name);
        symbol->ret_type = get_symbol_value_type(node->data.declfn.ret_type);
    }
    else
    {
        log_error("Symbol %s already defined.", name);
        exit(1);
    }

    bool prev_in_function = g_ctx.in_function;
    ptrdiff_t prev_stack_offset = g_ctx.stack_offset;
    const char* prev_function_name = g_ctx.current_function_name;
    symbol_value_t prev_return_type = g_ctx.expected_return_type;

    g_ctx.in_function = true;
    g_ctx.stack_offset = 0;
    g_ctx.current_function_name = name;
    g_ctx.expected_return_type = symbol->ret_type;

    EMIT(SECTION_GLOBAL, "global %s\n", name);
    EMIT(SECTION_TEXT, "%s:\n", name);

    // Standard prologue gives us a stable frame pointer so locals have fixed
    // offsets and call/return conventions stay consistent.
    x86_prologue();

    // Emit the body statements with the newly created function context.
    x86_block(node->data.declfn.block);

    g_ctx.in_function = prev_in_function;
    g_ctx.stack_offset = prev_stack_offset;
    g_ctx.current_function_name = prev_function_name;
    g_ctx.expected_return_type = prev_return_type;
    EXIT(DECLFN);
}

// Handles both declarations and reassignments by resolving the destination,
// type-checking the expression, and storing the value either globally or on
// the current stack frame.
void x86_assign(ast* node)
{
    ENTER(ASSIGN);

    // Emit the right hand side first (fully processing any expressions)
    ast* rhs = node->data.assign.rhs;
    symbol_value_t rhs_type = get_symbol_value_type(rhs);
    char* rhs_reg = x86_expr(rhs);

    ast* lhs = node->data.assign.lhs;
    char* name = NULL;
    symbol_t* symbol = NULL;

    switch (lhs->type)
    {
    // If it's a new variable, declare it
    case AST_DECLVAR:
        name = lhs->data.declvar.identifier->data.identifier.name;
        if (g_ctx.in_function)
        {
            // Locals consume stack slots inside the current function.
            symbol = symbol_define_local(name);
        }
        else
        {
            symbol = scope_lookup_shallow(g_ctx.global_scope, name);
            if (!symbol)
            {
                symbol = symbol_define_global(name);
            }
            // Global declarations reserve space in the data segment so they
            // can be addressed directly.
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

    // Fix up the symbol's value type the first time we encounter it and ensure
    // subsequent assignments respect the inferred/static type.
    if (symbol->value_type == SYMBOL_VALUE_UNKNOWN)
    {
        symbol->value_type = rhs_type;
    }
    else
    {
        ASSERT(symbol->value_type == rhs_type,
               "Cannot assign %s value to %s (expected %s).",
               symbol_value_to_string(rhs_type), name,
               symbol_value_to_string(symbol->value_type));
    }

    if (symbol->type == SYMBOL_GLOBAL)
    {
        // Globals live in memory, so store into the named label.
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

// Implements `if` / `else` branching by emitting labels and conditional jumps
// based on the evaluated condition register.
void x86_if(ast* node)
{
    ENTER(IF);
    ASSERT(node->type == AST_IF, "Expected IF node, got %s",
           ast_to_string(node->type));

    ast_if_stmt* stmt = &node->data.if_stmt;
    int label_id = g_ctx.branch_count++;

    // Evaluate the condition once and compare the result against zero.
    char* cond_reg = x86_expr(stmt->condition);
    char* else_label = NULL;
    char* end_label = formats(".Lendif_%d", label_id);

    EMIT(SECTION_TEXT, "\tcmp %s, 0\n", cond_reg);
    if (stmt->else_branch)
    {
        else_label = formats(".Lelse_%d", label_id);
        EMIT(SECTION_TEXT, "\tje %s\n", else_label);
    }
    else
    {
        EMIT(SECTION_TEXT, "\tje %s\n", end_label);
    }

    if (stmt->condition->type != AST_CALL)
    {
        register_unlock();
    }

    // Emit the `then` branch when the condition is truthy.
    x86_statement(stmt->then_branch);

    if (stmt->else_branch)
    {
        // Skip the else block after executing the then branch, mirroring high
        // level structured flow.
        EMIT(SECTION_TEXT, "\tjmp %s\n", end_label);
        EMIT(SECTION_TEXT, "%s:\n", else_label);
        x86_statement(stmt->else_branch);
    }

    EMIT(SECTION_TEXT, "%s:\n", end_label);

    free(end_label);
    if (else_label)
    {
        free(else_label);
    }
    EXIT(IF);
}

// Validates return types against the enclosing signature and moves the value
// into RAX before emitting the shared epilogue.
void x86_return(ast* node)
{
    ENTER(RET);
    ast* rhs = node->data.ret.node;
    symbol_value_t expected_type = g_ctx.expected_return_type;
    ASSERT(expected_type != SYMBOL_VALUE_UNKNOWN,
           "Return statement outside of a function context.");

    symbol_value_t actual_type =
        rhs ? get_symbol_value_type(rhs) : SYMBOL_VALUE_VOID;
    const char* fn_name = g_ctx.current_function_name
                              ? g_ctx.current_function_name
                              : "<anonymous>";

    // Enforce that void signatures never produce a value and non-void
    // signatures always return exactly one value of the right type.
    if (expected_type == SYMBOL_VALUE_VOID)
    {
        ASSERT(rhs == NULL || actual_type == SYMBOL_VALUE_VOID,
               "Function '%s' declared void cannot return a value.", fn_name);
    }
    else
    {
        ASSERT(rhs != NULL, "Function '%s' must return a %s value.", fn_name,
               symbol_value_to_string(expected_type));
        ASSERT(actual_type == expected_type,
               "Return type mismatch in function '%s' (expected %s, got %s).",
               fn_name, symbol_value_to_string(expected_type),
               symbol_value_to_string(actual_type));
    }

    if (rhs)
    {
        char* rhs_reg;

        // Only a handful of node types are valid return expressions; ensure
        // we delegate to the expression emitter for those shapes.
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
    }

    // Shared epilogue restores the stack frame and emits the final ret.
    x86_epilogue(true);
    EXIT(RET);
}

// Evaluates call arguments, marshals them into ABI-defined registers/stack
// slots, issues the call, and leaves the result in RAX.
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

// Emits a unique global label for the string literal and returns that symbol
// so expressions can reference it.
char* x86_string(char* text)
{
    // Create a new buffer for the line we're going to format. This is to
    // manage memory in a more efficient way.
    buffer_t* line = buffer_new();

    // Define the name as 'string_n' where 'n' is the current
    // string count.
    // Always define as bytes.
    char* string_name = formats("string_%d", g_ctx.string_count);
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
    g_ctx.string_count++;

    // Return the name of the string's symbol
    return string_name;
}

// Dispatches expression nodes to their specialized emitters and ensures the
// final value resides in a register for downstream instructions.
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

// Switchboard for statement-level nodes, ensuring each high-level construct
// routes to the correct emitter.
void x86_statement(ast* node)
{
    ENTER(STMT);
    // Dispatch to the appropriate emitter for each supported statement type.
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
    case AST_IF:
        x86_if(node);
        break;
    default:
        break;
    }
    EXIT(STMT);
}

// Emits every statement within a top-level body; bodies correspond to the
// root sequences under `program`.
void x86_body(ast* node)
{
    ASSERT(node->type == AST_BODY, "Wanted node type BODY, got %s", node->type);
    ENTER(BODY);
    for (size_t i = 0; i < node->data.body.count; i++)
    {
        ast* statement = node->data.body.statements[i];
        // Bodies emit statements sequentially, preserving source order.
        x86_statement(statement);
    }
    EXIT(BODY);
}

// Entry point for the backend: reinitializes global state, gathers globals,
// primes the assembly sections, and emits each top-level body.
void x86_program(ast* node)
{
    ASSERT(node->type == AST_PROGRAM, "Wanted node type PROGRAM, got %s",
           node->type);
    ENTER(PROGRAM);

    // Initialize scope state
    scope_free(g_ctx.global_scope);
    g_ctx.global_scope = scope_new(NULL);
    g_ctx.current_scope = g_ctx.global_scope;
    g_ctx.in_function = false;
    g_ctx.stack_offset = 0;
    g_ctx.branch_count = 0;
    g_ctx.current_function_name = NULL;
    g_ctx.expected_return_type = SYMBOL_VALUE_UNKNOWN;

    // Collect all global symbols prior to emitting any code.
    x86_globals(node);

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

    scope_free(g_ctx.global_scope);
    g_ctx.global_scope = NULL;
    g_ctx.current_scope = NULL;
    EXIT(PROGRAM);
}