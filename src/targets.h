#ifndef TARGETS_H
#define TARGETS_H

#include "buffer.h"

extern buffer_t* g_global;
extern buffer_t* g_data;
extern buffer_t* g_bss;
extern buffer_t* g_text;

#define B_DATA(...) buffer_printf(g_data, __VA_ARGS__)
#define B_BSS(...) buffer_printf(g_bss, __VA_ARGS__)
#define B_TEXT(...) buffer_printf(g_text, __VA_ARGS__)

typedef struct ast ast;

typedef enum target_type_t
{
    X86_32,
    X86_64,
} target_type_t;

static const char* target_type_to_string(target_type_t type)
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

typedef struct target_t
{
    void (*program)(ast*);
} target_t;

target_t* target_new(target_type_t type);
void target_free(target_t* target);

#endif