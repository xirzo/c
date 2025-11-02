#include "str.h"
#include <stdlib.h>
#include <string.h>

char *strdup(const char *str) {
    size_t len = strlen(str) + 1;
    char *copy = malloc(len);

    if (copy) {
        strcpy(copy, str);
    }

    return copy;
}
