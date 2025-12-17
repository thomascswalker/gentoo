#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

void log_debug(const char* format, ...);
void log_info(const char* format, ...);
void log_error(const char* format, ...);

#endif