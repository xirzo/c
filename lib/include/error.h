#ifndef ERROR_H
#define ERROR_H

#include <stdio.h>
#include "lexer.h"

typedef struct {
    const char *message;
    const char *filename;
    int line;
    int column;
    C_Token token;
} C_Error;

typedef struct {
    C_Error *errors;
} C_ErrorContext;

C_ErrorContext *C_ErrorContextCreate(void);
void C_ErrorContextFree(C_ErrorContext *ctx);

void C_ErrorReport(C_ErrorContext *ctx,
                    const char *message,
                    const char *filename,
                    int line,
                    int column);

void C_ErrorReportWithToken(C_ErrorContext *ctx,
                               const char *message,
                               C_Token token,
                               const char *filename);

void C_ErrorContextPrint(C_ErrorContext *ctx, FILE *output);

int C_ErrorContextHasErrors(C_ErrorContext *ctx);

#endif  // !ERROR_H
