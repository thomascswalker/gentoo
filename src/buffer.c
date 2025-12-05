#include "buffer.h"

#include <stdio.h>

#define BUFFER_INITIAL_CAPACITY 8
#define MAX_LOOP 10000
size_t LOOP_COUNT = 0;

buffer_t* buffer_new()
{
    buffer_t* buf = (buffer_t*)calloc(1, sizeof(buffer_t));
    buf->data = (char*)calloc(1, BUFFER_INITIAL_CAPACITY);
    buf->size = 0;
    return buf;
}

void buffer_free(buffer_t* buf)
{
    // Free the buffer data
    free(buf->data);

    // Free the buffer itself
    free(buf);
}

void buffer_realloc(buffer_t* buf)
{
    size_t new_size = buf->capacity * 2;
    char* new_data = (char*)malloc(new_size);
    memcpy(new_data, buf->data, buf->capacity);
    free(buf->data);
    buf->data = new_data;
    buf->capacity = new_size;
}

void buffer_putc(buffer_t* buf, char c)
{
    if ((buf->size + 1) == buf->capacity)
    {
        buffer_realloc(buf);
    }
    buf->data[buf->size++] = c;
}

void buffer_puts(buffer_t* buf, char* str)
{
    for (size_t i = 0; i < strlen(str); i++)
    {
        buffer_putc(buf, str[i]);
    }
}

void buffer_printf(buffer_t* buf, char* format, ...)
{
    va_list args;
    // while (1)
    // {
    size_t remaining = buf->capacity - buf->size;
    va_start(args, format);

    // Format `args` into `buf` with the specified `format`.
    size_t written = vsnprintf(buf->data + buf->size, remaining, format, args);
    va_end(args);
    if (remaining <= written)
    {
        buffer_realloc(buf);
        // continue;
    }
    buf->size += written;
    return;
    // }
}

char* buffer_vprintf(char* format, va_list in_args)
{
    buffer_t* buf = buffer_new();
    va_list args;
    LOOP_COUNT = 0;
    while (LOOP_COUNT < MAX_LOOP)
    {
        size_t remaining = buf->capacity - buf->size;
        va_copy(args, in_args);

        // Format `args` into `buf` with the specified `format`.
        size_t written = vsnprintf(buf->data + buf->size, remaining, format, args);
        va_end(args);
        if (remaining <= written)
        {
            buffer_realloc(buf);
            LOOP_COUNT++;
            continue;
        }
        buf->size += written;
        return buf->data;
    }
}

char* format(char* format, ...)
{
    va_list args;
    va_start(args, format);
    char* result = buffer_vprintf(format, args);
    va_end(args);
    return result;
}
