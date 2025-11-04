#ifndef ERROR_H
#define ERROR_H

#include <stdio.h>
#include "lexer.h"

typedef struct {
    const char *message;
    const char *filename;
    int line;
    int column;
    c_token token;
} c_error;

typedef struct {
    c_error *errors;
} c_error_context;

c_error_context *c_error_context_create(void);
void c_error_context_free(c_error_context *ctx);

void c_error_report(c_error_context *ctx,
                    const char *message,
                    const char *filename,
                    int line,
                    int column);

void c_error_report_with_token(c_error_context *ctx,
                               const char *message,
                               c_token token,
                               const char *filename);

void c_error_context_print(c_error_context *ctx, FILE *output);

int c_error_context_has_errors(c_error_context *ctx);

#endif  // !ERROR_H
