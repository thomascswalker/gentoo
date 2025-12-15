#ifndef X86_64_H
#define X86_64_H

#include <stdbool.h>

typedef enum x86_syscall_t
{
    X86_READ = 0,
    X86_WRITE,
    X86_OPEN,
    X86_CLOSE,
    X86_EXIT = 60
} x86_syscall_t;

// Distinguish between globals emitted in .data and locals that live on the
// stack.
typedef enum symbol_kind_t
{
    SYMBOL_GLOBAL,
    SYMBOL_LOCAL,
} symbol_kind_t;

typedef struct symbol_t
{
    const char* name;
    symbol_kind_t kind;
    ptrdiff_t stack_offset;
} symbol_t;

// Simple chained symbol table for lexical scopes.
typedef struct scope_t
{
    symbol_t* symbols;
    size_t count;
    size_t capacity;
    struct scope_t* parent;
} scope_t;

typedef struct ast ast;

// Allocate a new scope inheriting the parent's bindings.
scope_t* scope_create(scope_t* parent);
void scope_destroy(scope_t* scope);
void scope_push();
void scope_pop();
symbol_t* scope_lookup_shallow(scope_t* scope, const char* name);
symbol_t* scope_lookup(scope_t* scope, const char* name);
symbol_t* scope_add_symbol(scope_t* scope, const char* name,
                           symbol_kind_t kind);
ptrdiff_t allocate_stack_slot();
symbol_t* symbol_define_global(const char* name);
symbol_t* symbol_define_local(const char* name);
symbol_t* symbol_resolve(const char* name);
void collect_global_symbols(ast* node);

void x86_epilogue(bool emit_ret);
void x86_comment(char* text);
void x86_syscall(int code);
char* x86_expr(ast* node);
void x86_return(ast* node);
char* x86_call(ast* node);
void x86_assign(ast* node);
void x86_declvar(ast* node);
void x86_declfn(ast* node);
char* x86_binop(ast* node);
void x86_statement(ast* node);
void x86_body(ast* node);
void target_x86(ast* node);

#endif