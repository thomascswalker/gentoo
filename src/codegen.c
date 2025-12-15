#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "codegen.h"
#include "log.h"
#include "x86_64.h"

codegen_t* g_codegen = NULL;

void codegen_emit(section_type_t section, char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    switch (section)
    {
    case SECTION_GLOBAL:
        buffer_vprintf(g_codegen->global, fmt, args);
        break;
    case SECTION_TEXT:
        buffer_vprintf(g_codegen->text, fmt, args);
        break;
    case SECTION_DATA:
        buffer_vprintf(g_codegen->data, fmt, args);
        break;
    case SECTION_BSS:
        buffer_vprintf(g_codegen->bss, fmt, args);
        break;
    default:
        break;
    }
    va_end(args);
}

codegen_t* codegen_new(codegen_type_t type)
{
    const codegen_t* template = NULL;

    // Depending on the target architecture, set the respective
    // codegen struct for emitting the correct assembly for that
    // architecture.
    switch (type)
    {
    case X86_32:
    case X86_64:
    {
        template = &CODEGEN_X86_64;
        break;
    }
    default:
        // If no architecture is specified or it's invalid, free
        // the target object and return null.
        return NULL;
    }

    g_codegen = (codegen_t*)calloc(1, sizeof(codegen_t));
    if (!g_codegen)
    {
        log_error("Failed to allocate codegen struct.");
        return NULL;
    }

    *g_codegen = *template;

    g_codegen->emit = codegen_emit;

    log_debug("Allocating section buffers...");
    g_codegen->global = buffer_new();
    g_codegen->data = buffer_new();
    g_codegen->text = buffer_new();
    g_codegen->bss = buffer_new();
    log_debug("Completed section buffer allocation.");

    return g_codegen;
}

void codegen_free(codegen_t* codegen)
{
    if (!codegen)
    {
        return;
    }
    buffer_free(codegen->global);
    buffer_free(codegen->data);
    buffer_free(codegen->text);
    buffer_free(codegen->bss);
    free(codegen);
    g_codegen = NULL;
}
