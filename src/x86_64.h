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

typedef enum symbol_type_t
{
    SYMBOL_GLOBAL,
    SYMBOL_LOCAL,
} symbol_type_t;

static char* symbol_type_to_string(symbol_type_t type)
{
    if (type == SYMBOL_GLOBAL)
    {
        return "GLOBAL";
    }
    return "LOCAL";
}

typedef struct symbol_t
{
    const char* name;
    symbol_type_t type;
    ptrdiff_t offset; // Stack offset
} symbol_t;

static char* symbol_to_string(symbol_t* symbol)
{
    return formats("'%s', %s, 0x%02x", symbol->name,
                   symbol_type_to_string(symbol->type), symbol->offset);
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

scope_t* scope_new(scope_t* parent);
void scope_free(scope_t* scope);
void scope_push();
void scope_pop();
symbol_t* scope_lookup_shallow(scope_t* scope, const char* name);
symbol_t* scope_lookup(scope_t* scope, const char* name);
symbol_t* scope_add_symbol(scope_t* scope, const char* name,
                           symbol_type_t type);
ptrdiff_t allocate_stack_slot();
symbol_t* symbol_define_global(const char* name);
symbol_t* symbol_define_local(const char* name);
symbol_t* symbol_resolve(const char* name);
void collect_global_symbols(ast* node);

/* Assembly */

void x86_program(ast* node);
void x86_body(ast* node);
void x86_statement(ast* node);
char* x86_binop(ast* node);
void x86_declfn(ast* node);
void x86_declvar(ast* node);
void x86_assign(ast* node);
char* x86_call(ast* node);
void x86_return(ast* node);
char* x86_expr(ast* node);
void x86_syscall(int code);
void x86_comment(char* text);
void x86_epilogue(bool emit_ret);
void x86_prologue();

extern codegen_t CODEGEN_X86_64;

#endif