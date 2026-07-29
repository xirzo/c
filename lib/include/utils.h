#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

#define EXIT_WITH_ERROR(...)      \
  do {                            \
    fprintf(stderr, __VA_ARGS__); \
    exit(1);                      \
  } while (1)

#if defined(ENABLE_LOGGING) && ENABLE_LOGGING == 1
#define LOG_DEBUG(fmt, ...) printf("[DEBUG] %s:%d: " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) ((void)0)
#endif

#endif  // !UTILS_H
