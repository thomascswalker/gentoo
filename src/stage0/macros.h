#ifndef MACROS_H
#define MACROS_H

#include <stdlib.h>

#include "log.h"

#define ASSERT(cond, ...)                                                      \
    if (!(cond))                                                               \
    {                                                                          \
        log_error(__VA_ARGS__);                                                \
        exit(1);                                                               \
    }

#endif