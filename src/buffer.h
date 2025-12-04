#ifndef BUFFER_H
#define BUFFER_H

#include <stdlib.h>
#include <string.h>

typedef struct buffer_t
{
    // Character data within this buffer
    char* data;
    // The character count within this buffer
    size_t size;
    // The current capacity of this buffer in bytes
    size_t capacity;
} buffer_t;

buffer_t* buffer_new();
void buffer_free(buffer_t* buf);
void buffer_realloc(buffer_t* buf);
void buffer_putc(buffer_t* buf, char c);
void buffer_puts(buffer_t* buf, char* str);
void buffer_printf(buffer_t* buf, char* format, ...);
char* buffer_vprintf(char* format, va_list in_args);
char* format(char* format, ...);

#endif