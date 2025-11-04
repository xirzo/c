#ifndef STR_H
#define STR_H

#define MAX_SOURCE_SIZE (1024 * 1024)

char *strdup(const char *str);
char *read_file_to_buffer(const char *filename);

#endif  // !STR_H
