#include <stddef.h>

#include "log.h"
#include "targets.h"
#include "x86_64.h"

buffer_t* g_global;
buffer_t* g_data;
buffer_t* g_bss;
buffer_t* g_text;

ast_emitter_t get_ast_emitter(target_type_t type)
{
    log_info("Emitting %s ASM", target_type_to_string(type));

    // Allocate the buffers for each section
    g_global = buffer_new();
    g_data = buffer_new();
    g_bss = buffer_new();
    g_text = buffer_new();

    // Depending on the target architecture, set the respective
    // function for emitting the correct assembly for that
    // architecture.
    switch (type)
    {
    case X86_64:
        return target_x86;
    case X86_32:
    default:
        // If no architeceture is specified or it's invalid, free
        // the target object and return null.
        return NULL;
    }
}
