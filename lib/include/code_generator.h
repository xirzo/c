#ifndef CODE_GENERATOR
#define CODE_GENERATOR

#include "parser.h"

// TODO: fix naming conflicts
// (maybe should not name variabls
// rather just identify them by
// memory pos)

char **C_CodeGenEmit(C_AstProgram *program);
char **C_CodeGenEmitFunctionDeclaration(
    C_AstFunctionDeclaration *function_declaration);
char **C_CodeGenEmitFunctionCall(C_AstFunctionCall *function_call,
                                 const char        *assign_to_variable);
// TODO: maybe add some context structure
char **C_CodeGenEmitStatement(C_AstStatement *statement, int *current_offset);
char **C_CodeGenEmitBlock(C_AstBlock *block, int *current_offset);
char **C_CodeGenEmitReturn(C_AstReturn *ret, int *current_offset);
char **C_CodeGenEmitBinaryExpression(C_AstBinaryExpression *binary,
                                     int                   *current_offset);
char **C_CodeGenEmitExpression(C_AstExpression *expression,
                               int             *current_offset);
char **C_CodeGenEmitVariable(C_AstVariable *variable);

#endif  // !CODE_GENERATOR
