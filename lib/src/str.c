#include "str.h"
#include <stdio.h>
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

char *read_file_to_buffer(const char *filename) {
    FILE *file = fopen(filename, "r");

    if (!file) {
        fprintf(stderr, "Failed to open source file: %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);

    if (filesize < 0 || filesize > MAX_SOURCE_SIZE) {
        fprintf(stderr, "Source file too large or invalid size.\n");
        fclose(file);
        return NULL;
    }

    rewind(file);
    char *buffer = malloc(filesize + 1);

    if (!buffer) {
        fprintf(stderr, "Failed to allocate memory for source buffer.\n");
        fclose(file);
        return NULL;
    }

    size_t read_len = fread(buffer, 1, filesize, file);
    buffer[read_len] = '\0';
    fclose(file);
    return buffer;
}
