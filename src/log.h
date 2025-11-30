#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <stdio.h>

static void log_debug(char* format, ...)
{
    printf("%s", "\033[30m[DBG] - ");

    va_list args;
    va_start(args, format);
    vprintf(format, args);

    printf("%s", "\033[0m\n");

    va_end(args);
}

static void log_info(char* format, ...)
{
    printf("%s", "\033[0m[INF] - ");

    va_list args;
    va_start(args, format);
    vprintf(format, args);

    printf("%s", "\033[0m\n");

    va_end(args);
}

static void log_error(char* format, ...)
{
    printf("%s", "\033[31m[ERR] - ");

    va_list args;
    va_start(args, format);
    vprintf(format, args);

    printf("%s", "\033[0m\n");

    va_end(args);
}

#endif