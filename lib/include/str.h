#ifndef STR_H
#define STR_H

#define MAX_SOURCE_SIZE (1024 * 1024)

char *strdup(const char *str);
char *C_ReadFileToBuffer(const char *filename);

#endif  // !STR_H
