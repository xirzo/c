#ifndef CODE_GENERATOR
#define CODE_GENERATOR

#include "parser.h"

// TODO: fix naming conflicts
// (maybe should not name variabls
// rather just identify them by
// memory pos)

char **c_code_gen_emit(c_ast_program *program);
char **c_code_gen_emit_function_declaration(
    c_ast_function_declaration *function_declaration);
char **c_code_gen_emit_function_call(c_ast_function_call *function_call,
                                     const char *assign_to_variable);
// TODO: maybe add some context structure
char **c_code_gen_emit_statement(c_ast_statement *statement,
                                 int *current_offset);
char **c_code_gen_emit_block(c_ast_block *block, int *current_offset);
char **c_code_gen_emit_return(c_ast_return *ret, int *current_offset);
char **c_code_gen_emit_binary_expression(c_ast_binary_expression *binary,
                                         int *current_offset);
char **c_code_gen_emit_expression(c_ast_expression *expression,
                                  int *current_offset);
char **c_code_gen_emit_variable(c_ast_variable *variable);

#endif  // !CODE_GENERATOR
