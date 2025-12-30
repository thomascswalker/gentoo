#ifndef X86_64_H
#define X86_64_H

#include <stdbool.h>
#include <stddef.h>

typedef enum x86_syscall_t
{
    X86_READ = 0,
    X86_WRITE,
    X86_OPEN,
    X86_CLOSE,
    X86_EXIT = 60
} x86_syscall_t;

typedef enum symbol_scope_t
{
    SYMBOL_GLOBAL,
    SYMBOL_LOCAL,
} symbol_scope_t;

typedef enum symbol_value_t
{
    SYMBOL_VALUE_UNKNOWN,
    SYMBOL_VALUE_VOID,
    SYMBOL_VALUE_INT,
    SYMBOL_VALUE_BOOL,
    SYMBOL_VALUE_STRING,
    SYMBOL_VALUE_FN,
} symbol_value_t;

// Returns a printable name for the provided symbol type.
static char* symbol_type_to_string(symbol_scope_t type)
{
    return type == SYMBOL_GLOBAL ? "GLOBAL" : "LOCAL";
}

static char* symbol_value_to_string(symbol_value_t kind)
{
    switch (kind)
    {
    case SYMBOL_VALUE_INT:
        return "INT";
    case SYMBOL_VALUE_BOOL:
        return "BOOL";
    case SYMBOL_VALUE_STRING:
        return "STRING";
    case SYMBOL_VALUE_FN:
        return "FN";
    case SYMBOL_VALUE_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

typedef struct symbol_t
{
    const char* name;
    symbol_scope_t type;
    symbol_value_t value_kind;
    symbol_value_t ret_kind;
    ptrdiff_t offset; // Stack offset
} symbol_t;

// Formats a symbol into a human-readable string for logging/debugging.
static char* symbol_to_string(symbol_t* symbol)
{
    return formats("'%s', %s, %s, 0x%02x", symbol->name,
                   symbol_type_to_string(symbol->type),
                   symbol_value_to_string(symbol->value_kind), symbol->offset);
}

typedef struct scope_t
{
    symbol_t* symbols;
    size_t count;
    size_t capacity;
    struct scope_t* parent;
} scope_t;

typedef struct ast ast;

/* Scope */

// Allocates a new scope that inherits the bindings of the parent scope.
scope_t* scope_new(scope_t* parent);
// Releases all memory owned by the provided scope.
void scope_free(scope_t* scope);
// Pushes a new child scope onto the scope stack.
void scope_push();
// Pops the current scope, restoring its parent.
void scope_pop();
// Searches only the specified scope for a `symbol:name` match.
symbol_t* scope_lookup_shallow(scope_t* scope, const char* name);
// Walks parent scopes until the requested symbol is located.
symbol_t* scope_lookup(scope_t* scope, const char* name);
// Adds a symbol into the specified scope.
symbol_t* scope_add_symbol(scope_t* scope, const char* name,
                           symbol_scope_t type);

// Allocates space on the current stack frame and returns the new offset.
ptrdiff_t allocate_stack_slot();
// Declares a symbol in the global scope table.
symbol_t* symbol_define_global(const char* name);
// Declares a symbol that belongs to the current local scope.
symbol_t* symbol_define_local(const char* name);
// Resolves a symbol name and asserts it exists within reachable scopes.
symbol_t* symbol_resolve(const char* name);
// Given the specified AST node, returns the evaluated symbol value kind.
symbol_value_t get_symbol_value_kind(ast* node);

/* Assembly */

// Scans the AST for global definitions and records them for later codegen.
void x86_globals(ast* node);
void x86_program(ast* node);
void x86_body(ast* node);
void x86_statement(ast* node);
void x86_block(ast* node);
char* x86_binop(ast* node);
void x86_if(ast* node);
void x86_declfn(ast* node);
void x86_declvar(ast* node);
void x86_assign(ast* node);
char* x86_call(ast* node);
char* x86_string(char* text);
void x86_return(ast* node);
char* x86_expr(ast* node);
void x86_syscall(int code);
void x86_comment(char* text);
void x86_epilogue(bool emit_ret);
void x86_prologue();

extern codegen_t CODEGEN_X86_64;

#endif