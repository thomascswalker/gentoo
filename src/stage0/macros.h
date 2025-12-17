#ifndef MACROS_H
#define MACROS_H

#include "log.h"
#include <stdlib.h>

#define ASSERT(cond, ...)                                                      \
    if (!(cond))                                                               \
    {                                                                          \
        log_error(__VA_ARGS__);                                                \
        exit(1);                                                               \
    }

#endif