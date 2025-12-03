#include <stdlib.h>

#include "ast.h"

typedef enum buffer_state
{
    WRITEABLE,
    EXECUTABLE,
} buffer_state;

typedef struct
{
    uint8_t* address;
    buffer_state state;
    size_t size;
    size_t capacity;
} buffer_t;

uint8_t* buffer_alloc_writeable(size_t capacity)
{
    return (uint8_t*)malloc(capacity);
}

void buffer_init(buffer_t* result, size_t capacity)
{
    result->address = buffer_alloc_writeable(capacity);
    result->state = WRITEABLE;
    result->size = 0;
    result->capacity = capacity;
}

void codegen(ast* node);