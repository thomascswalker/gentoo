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
    if (!new_data)
    {
        return;
    }
    size_t to_copy = buf->size;
    if (to_copy > 0)
    {
        memcpy(new_data, buf->data, to_copy);
    }
    new_data[to_copy] = '\0';
    free(buf->data);
    buf->data = new_data;
    buf->capacity = new_capacity;
}

void buffer_recalloc(buffer_t* buf, size_t new_capacity)
{
    if (new_capacity == 0)
    {
        return;
    }
    char* new_data = (char*)malloc(new_capacity);
    if (!new_data)
    {
        return;
    }
    size_t to_copy = buf->size;
    if (to_copy >= new_capacity)
    {
        to_copy = new_capacity - 1;
    }
    if (to_copy > 0)
    {
        memcpy(new_data, buf->data, to_copy);
    }
    new_data[to_copy] = '\0';
    buf->size = to_copy;
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

    // Reallocate memory by doubling it until we have enough
    // memory (size) plus 1 (to account for the NULL terminator)
    while (buf->size + 1 >= buf->capacity)
    {
        size_t new_capacity = buf->capacity * 2;
        buffer_recalloc(buf, new_capacity);
    }

    // Actually set the character at the current buffer size
    buf->data[buf->size] = c;

    // Append the NULL terminator to the end
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
    // Need room for the string plus terminating null
    size_t remaining = buf->capacity - buf->size;
    if (remaining <= length)
    {
        size_t needed = buf->size + length + 1;
        size_t new_capacity = buf->capacity * 2;
        while (new_capacity < needed)
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

    buf->data[buf->size] = '\0';
}

void buffer_printf(buffer_t* buf, char* format, ...)
{
    va_list args;
    while (1)
    {
        size_t remaining = buf->capacity - buf->size;
        va_start(args, format);

        // Format `args` into `buf` with the specified `format`.
        size_t written =
            vsnprintf(buf->data + buf->size, remaining, format, args);
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

void buffer_vprintf(buffer_t* buf, char* format, va_list in_args)
{
    va_list args;
    while (1)
    {
        size_t remaining = buf->capacity - buf->size;
        va_copy(args, in_args);

        // Format `args` into `buf` with the specified `format`.
        size_t written =
            vsnprintf(buf->data + buf->size, remaining, format, args);
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

char* formats(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    // Use vsnprintf to determine the required size
    // We pass NULL for the buffer and 0 for the size to get the length
    int size = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (size < 0)
    {
        return NULL; // Error in formatting
    }

    // Allocate memory for the string (including the null terminator)
    char* buffer = malloc(size + 1);
    if (buffer == NULL)
    {
        return NULL; // Memory allocation failed
    }

    // Format the string into the allocated buffer
    va_start(args, format);
    vsnprintf(buffer, size + 1, format, args);
    va_end(args);

    return buffer;
}