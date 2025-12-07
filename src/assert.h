#include "log.h"

#define assert(cond, msg, ...)                                                 \
    if (!(cond))                                                               \
    {                                                                          \
        log_error(msg, __VA_ARGS__);                                           \
        exit(1);                                                               \
    }

//