#include "log.h"

#include <stdarg.h>
#include <stdio.h>

void log_debug(const char* format, ...)
{
#ifdef _DEBUG
    printf("%s", "\033[30m[DBG] - ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    printf("%s", "\033[0m\n");
    va_end(args);
#endif
}

void log_info(const char* format, ...)
{
    printf("%s", "\033[0m[INF] - ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    printf("%s", "\033[0m\n");
    va_end(args);
}

void log_error(const char* format, ...)
{
    printf("%s", "\033[31m[ERR] - ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    printf("%s", "\033[0m\n");
    va_end(args);
}
