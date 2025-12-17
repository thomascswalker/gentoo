#include <stddef.h>
#include <string.h>

#include "macros.h"
#include "reg.h"

reg_t g_registers[REG_COUNT] = {
    {"rax", false}, //
    {"rbx", false}, //
    {"rcx", false}, //
    {"rdx", false}, //
    {"rsi", false}, //
    {"rdi", false}, //
    {"r8", false},  //
    {"r9", false},  //
    {"r10", false}, //
    {"r11", false}, //
    {"r12", false}, //
    {"r13", false}, //
    {"r14", false}, //
    {"r15", false}, //
};
size_t g_lock_count = 0;
size_t g_unlock_count = 0;

reg_t* register_get(char* name)
{
    for (int i = 0; i < REG_COUNT; i++)
    {
        if (strcmp(g_registers[i].name, name) == 0)
        {
            return &g_registers[i];
        }
    }
    return NULL;
}

void register_assert()
{
    ASSERT(g_unlock_count <= g_lock_count,
           "Unlock can never be greater than lock!: Lock:%d > Unlock:%d",
           g_lock_count, g_unlock_count);
}

char* register_lock()
{
    for (int i = 0; i < REG_COUNT; i++)
    {
        reg_t* reg = &g_registers[i];
        if (reg->locked == false)
        {
            g_lock_count++;
            register_assert();
            reg->locked = true;

            return reg->name;
        }
    }
    return NULL;
}

char* register_unlock()
{
    // Release the most-recently locked register (last-in, first-out).
    // Start from the end so that we free the last locked register first.
    for (int i = REG_COUNT - 1; i >= 0; i--)
    {
        reg_t* reg = &g_registers[i];
        if (reg->locked == true)
        {
            g_unlock_count++;
            register_assert();
            reg->locked = false;
            return reg->name;
        }
    }
    return NULL;
}
