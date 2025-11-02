#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

#define EXIT_WITH_ERROR(error)  \
    do {                        \
        fprintf(stderr, error); \
        exit(1);                \
    } while (1)

#define EXIT_WITH_ERROR_ARGS(error, ...)     \
    do {                                     \
        fprintf(stderr, error, __VA_ARGS__); \
        exit(1);                             \
    } while (1)

#if defined(ENABLE_LOGGING) && ENABLE_LOGGING == 1
#define LOG_DEBUG(...) printf(__VA_ARGS__)
#else
#define LOG_DEBUG(...)
#endif

#endif  // !UTILS_H
