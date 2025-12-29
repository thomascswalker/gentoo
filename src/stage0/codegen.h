#ifndef TARGETS_H
#define TARGETS_H

#include <stdbool.h>

#include "buffer.h"

typedef struct ast ast;

typedef enum codegen_type_t
{
    X86_32,
    X86_64,
} codegen_type_t;

typedef enum section_type_t
{
    SECTION_GLOBAL,
    SECTION_BSS,
    SECTION_TEXT,
    SECTION_DATA
} section_type_t;

typedef struct codegen_section_t
{
    section_type_t type;
    buffer_t* buffer;
} codegen_section_t;

typedef struct codegen_ops_t
{
    void (*program)(ast* node);
    void (*body)(ast* node);
    void (*statement)(ast* node);
    char* (*binop)(ast* node);
    void (*declfn)(ast* node);
    void (*declvar)(ast* node);
    void (*assign)(ast* node);
    char* (*call)(ast* node);
    char* (*string)(char* text);
    void (*ret)(ast* node);
    char* (*expr)(ast* node);
    void (*syscall)(int code);
    void (*comment)(char* text);
    void (*prologue)();
    void (*epilogue)(bool emit_ret);
} codegen_ops_t;

typedef struct codegen_t
{
    codegen_type_t type;
    codegen_ops_t ops;

    void (*emit)(section_type_t section, char* fmt, ...);

    buffer_t* global;
    buffer_t* data;
    buffer_t* text;
    buffer_t* bss;
} codegen_t;

extern codegen_t* g_codegen;

static const char* codegen_type_to_string(codegen_type_t type)
{
    switch (type)
    {
    case X86_32:
        return "x86-32";
    case X86_64:
        return "x86-64";
    default:
        break;
    }

    return "";
}

// Constructs the appropriate `codegen_t` object based on the specified `type`.
codegen_t* codegen_new(codegen_type_t type);
// Frees all buffers within the codegen object and then frees the object itself.
void codegen_free(codegen_t* codegen);
// Emits the formatted string to the corresponding ASM `section`.
void codegen_emit(section_type_t section, char* fmt, ...);

// Macro to simplify emitting ASM
#define EMIT g_codegen->emit

#endif