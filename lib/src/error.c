#include "error.h"
#include "stb_ds.h"
#include "str.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

c_error_context *c_error_context_create(void) {
    c_error_context *context = malloc(sizeof(c_error_context));

    if (!context) {
        return NULL;
    }

    context->errors = NULL;

    return context;
}

void c_error_context_free(c_error_context *ctx) {
    if (!ctx) {
        return;
    }

    for (int i = 0; i < arrlen(ctx->errors); i++) {
        free((void *)ctx[i].errors->message);
    }

    arrfree(ctx->errors);
    free(ctx);
}

void c_error_report(c_error_context *ctx,
                    const char *message,
                    const char *filename,
                    int line,
                    int column) {
    size_t old_length = arrlenu(ctx->errors);

    arrput(ctx->errors, (c_error){0});

    c_error *error = &ctx->errors[old_length];

    error->message = strdup(message);

    if (!error->message) {
        arrdel(ctx->errors, old_length);
        assert("ERROR MEMORY ALLOCATION FAILED");
        return;
    }

    error->filename = filename;
    error->line = line;
    error->column = column;
}

void c_error_report_with_token(c_error_context *ctx,
                               const char *message,
                               c_token token,
                               const char *filename) {
    size_t old_length = arrlenu(ctx->errors);

    arrput(ctx->errors, (c_error){0});

    c_error *error = &ctx->errors[old_length];

    error->message = strdup(message);

    if (!error->message) {
        arrdel(ctx->errors, old_length);
        assert("ERROR MEMORY ALLOCATION FAILED");
        return;
    }

    error->filename = filename;
    error->line = token.line;
    error->token = token;
    error->column = token.column;
}

void c_error_context_print(c_error_context *ctx, FILE *output) {
    // NOTE: maybe add lines printing later with caret
    for (int i = 0; i < arrlen(ctx->errors); i++) {
        c_error *error = &ctx->errors[i];

        fprintf(output,
                "%s:%d:%d: error: %s\n",
                error->filename,
                error->line,
                error->column,
                error->message);
    }
}

int c_error_context_has_errors(c_error_context *ctx) {
    return arrlen(ctx->errors) > 0;
}
