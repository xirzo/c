#ifndef CODE_GENERATOR
#define CODE_GENERATOR

#include "parser.h"
#include "xi_string.h"

String *C_CodeGenEmit(C_AstProgram *program);
String *C_CodeGenEmitFunctionDeclaration(
    C_AstFunctionDeclaration *function_declaration);
String *C_CodeGenEmitFunctionCall(C_AstFunctionCall *function_call,
                                  const String      *assign_to_variable);
String *C_CodeGenEmitStatement(C_AstStatement *statement, int *current_offset);
String *C_CodeGenEmitBlock(C_AstBlock *block, int *current_offset);
String *C_CodeGenEmitReturn(C_AstReturn *ret, int *current_offset);
String *C_CodeGenEmitBinaryExpression(C_AstBinaryExpression *binary,
                                      int                   *current_offset);
String *C_CodeGenEmitExpression(C_AstExpression *expression,
                                int             *current_offset);
String *C_CodeGenEmitVariable(C_AstVariable *variable);

#endif  // !CODE_GENERATOR
