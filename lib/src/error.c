#include "error.h"
#include "stb_ds.h"
#include "xi_string.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

C_ErrorContext *C_ErrorContextCreate(void) {
    C_ErrorContext *context = malloc(sizeof(C_ErrorContext));

    if (!context) {
        return NULL;
    }

    context->errors = NULL;

    return context;
}

void C_ErrorContextFree(C_ErrorContext *ctx) {
    if (!ctx) {
        return;
    }

    for (int i = 0; i < arrlen(ctx->errors); i++) {
        StringFree(&ctx->errors[i].message);
    }

    arrfree(ctx->errors);
    free(ctx);
}

void C_ErrorReport(C_ErrorContext *ctx,
                    const char *message,
                    const char *filename,
                    int line,
                    int column) {
    size_t old_length = arrlenu(ctx->errors);

    arrput(ctx->errors, (C_Error){0});

    C_Error *error = &ctx->errors[old_length];

    error->message = StringCreate(message);

    if (!error->message.data) {
        arrdel(ctx->errors, old_length);
        assert("ERROR MEMORY ALLOCATION FAILED");
        return;
    }

    error->filename = filename;
    error->line = line;
    error->column = column;
}

void C_ErrorReportWithToken(C_ErrorContext *ctx,
                               const char *message,
                               C_Token token,
                               const char *filename) {
    size_t old_length = arrlenu(ctx->errors);

    arrput(ctx->errors, (C_Error){0});

    C_Error *error = &ctx->errors[old_length];

    error->message = StringCreate(message);

    if (!error->message.data) {
        arrdel(ctx->errors, old_length);
        assert("ERROR MEMORY ALLOCATION FAILED");
        return;
    }

    error->filename = filename;
    error->line = token.line;
    error->token = token;
    error->column = token.column;
}

void C_ErrorContextPrint(C_ErrorContext *ctx, FILE *output) {
    // NOTE: maybe add lines printing later with caret
    for (int i = 0; i < arrlen(ctx->errors); i++) {
        C_Error *error = &ctx->errors[i];

        fprintf(output,
                "%s:%d:%d: error: %s\n",
                error->filename,
                error->line,
                error->column,
                StringGetCstr(&error->message));
    }
}

int C_ErrorContextHasErrors(C_ErrorContext *ctx) {
    return arrlen(ctx->errors) > 0;
}
