#include "buffer.h"
#include "log.h"

#include <stdio.h>

#define BUFFER_INITIAL_CAPACITY 8

buffer_t* buffer_new()
{
    buffer_t* buf = (buffer_t*)calloc(1, sizeof(buffer_t));
    buf->data = (char*)calloc(1, BUFFER_INITIAL_CAPACITY);
    buf->size = 0;
    buf->capacity = BUFFER_INITIAL_CAPACITY;
    return buf;
}

void buffer_free(buffer_t* buf)
{
    if (buf == NULL)
    {
        return;
    }

    if (buf->data != NULL)
    {
        // Free the buffer data
        free(buf->data);

        buf->data = NULL;
    }

    // Free the buffer itself
    free(buf);
}

void buffer_realloc(buffer_t* buf)
{
    size_t new_capacity = buf->capacity * 2;
    char* new_data = (char*)malloc(new_capacity);
    memcpy(new_data, buf->data, buf->capacity);
    free(buf->data);
    buf->data = new_data;
    buf->capacity = new_capacity;
}

void buffer_recalloc(buffer_t* buf, size_t new_capacity)
{
    char* new_data = (char*)malloc(new_capacity);
    memcpy(new_data, buf->data, buf->capacity);
    free(buf->data);
    buf->data = new_data;
    buf->capacity = new_capacity;
}

void buffer_putc(buffer_t* buf, char c)
{
    if (buf == NULL)
    {
        return;
    }

    while (buf->size == buf->capacity)
    {
        size_t new_capacity = buf->capacity * 2;
        buffer_recalloc(buf, new_capacity);
    }
    buf->data[buf->size] = c;
    buf->size++;
    buf->data[buf->size] = 0;
}

void buffer_puts(buffer_t* buf, char* str)
{
    if (buf == NULL || str == NULL)
    {
        return;
    }

    size_t length = strlen(str);
    log_info("Length: %d", length);
    size_t remaining = buf->capacity - buf->size;
    log_info("Remaining: %d", remaining);

    if (remaining < length)
    {
        size_t new_size = buf->size + length;
        size_t new_capacity = buf->capacity * 2;
        while (new_capacity < new_size)
        {
            new_capacity *= 2;
        }
        buffer_recalloc(buf, new_capacity);
    }

    for (size_t i = 0; i < length; i++)
    {
        buf->data[buf->size] = str[i];
        buf->size++;
    }

    buf->data[buf->size] = 0;
}

void buffer_printf(buffer_t* buf, char* format, ...)
{
    va_list args;
    while (1)
    {
        size_t remaining = buf->capacity - buf->size;
        va_start(args, format);

        // Format `args` into `buf` with the specified `format`.
        size_t written = vsnprintf(buf->data + buf->size, remaining, format, args);
        va_end(args);
        if (remaining <= written)
        {
            buffer_realloc(buf);
            continue;
        }
        buf->size += written;
        return;
    }
}

char* buffer_vprintf(char* format, va_list in_args)
{
    buffer_t* buf = buffer_new();
    va_list args;
    while (1)
    {
        size_t remaining = buf->capacity - buf->size;
        va_copy(args, in_args);

        // Format `args` into `buf` with the specified `format`.
        size_t written = vsnprintf(buf->data + buf->size, remaining, format, args);
        va_end(args);
        if (remaining <= written)
        {
            buffer_realloc(buf);
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
