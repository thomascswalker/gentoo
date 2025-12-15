#include "log.h"

#define assert(cond, ...)                                                      \
    if (!(cond))                                                               \
    {                                                                          \
        log_error(__VA_ARGS__);                                                \
        exit(1);                                                               \
    }

//