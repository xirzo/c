#ifndef PARSER_H
#define PARSER_H

#include "error.h"
#include "lexer.h"

typedef struct c_ast_expression c_ast_expression;
typedef struct c_ast_statement c_ast_statement;

typedef enum {
    // NOTE: maybe should separate
    // into different types of literals
    C_CONSTANT,
    C_FUNCTION_CALL,
    C_VARIABLE,
    C_BINARY_EXPRESSION,
} c_ast_expression_type;

typedef struct {
    // NOTE: for now only int
    int value;
} c_ast_constant;

typedef struct {
    char *function_name;
} c_ast_function_call;

typedef struct {
    char *name;
} c_ast_variable;

typedef struct {
    char symbol;
    c_ast_expression *lhs;
    c_ast_expression *rhs;
} c_ast_binary_expression;

typedef struct c_ast_expression {
    c_ast_expression_type type;

    union {
        c_ast_constant *constant;
        c_ast_function_call *function_call;
        c_ast_variable *variable;
        c_ast_binary_expression *binary;
    };
} c_ast_expression;

typedef enum {
    C_STATEMENT_BLOCK,
    C_STATEMENT_RETURN,
    C_STATEMENT_FUNCTION_DECLARATION,
    C_STATEMENT_EXPRESSION,
    C_STATEMENT_ASSIGNMENT,
    C_STATEMENT_NOOP,
} c_ast_statement_type;

typedef struct {
    c_ast_statement **statements;
} c_ast_block;

typedef struct {
    c_ast_expression *value;
} c_ast_return;

typedef struct {
    char *function_name;
    c_ast_block *body;
} c_ast_function_declaration;

typedef struct {
    char *variable_name;
    c_ast_expression *expression;
} c_ast_variable_assignment;

typedef struct c_ast_statement {
    c_ast_statement_type type;

    union {
        c_ast_block *block;
        c_ast_return *return_statement;
        c_ast_function_declaration *function_declaration;
        c_ast_expression *expression;
        c_ast_variable_assignment *assignment;
    };
} c_ast_statement;

typedef struct {
    c_ast_function_declaration **function_declarations;
} c_ast_program;

typedef struct {
    double left;
    double right;
} c_infix_binding_power;

typedef struct {
    c_error_context *error_context;
    const char *filename;
    c_token *tokens;
    c_token current_token;
    size_t current_position;
    size_t read_position;
} c_parser;

c_parser *c_parser_create(c_token *tokens,
                          c_error_context *error_context,
                          const char *filename);
c_ast_program *c_parser_parse(c_parser *parser);
void c_parser_free(c_parser *parser);
void c_parser_free_program(c_ast_program *program);

// NOTE: in fact not a part of public api, but can be used
void c_parser_advance(c_parser *parser);
c_token c_parser_peek(c_parser *parser);

c_ast_statement *c_parser_parse_statement(c_parser *parser);
c_ast_variable_assignment *c_parser_parse_variable_assignment(c_parser *parser);
c_infix_binding_power c_get_infix_binding_power(c_token_type token_type);
c_ast_expression *c_parser_parse_expression(c_parser *parser);
c_ast_expression *c_parser_parse_expression_with_precedence(
    c_parser *parser,
    double min_binding_power);
c_ast_constant *c_parser_parse_constant(c_parser *parser);
c_ast_function_call *c_parser_parse_function_call(c_parser *parser);
c_ast_return *c_parser_parse_return(c_parser *parser);
c_ast_block *c_parser_parse_block(c_parser *parser);
c_ast_function_declaration *c_parser_parse_function_declaration(
    c_parser *parser);
c_ast_variable *c_parser_parse_variable(c_parser *parser);

void c_ast_free_expression(c_ast_expression *expression);
void c_ast_free_statement(c_ast_statement *statement);
void c_ast_free_block(c_ast_block *block);
void c_ast_free_return(c_ast_return *ret);
void c_ast_free_variable_assignment(c_ast_variable_assignment *assignment);
void c_ast_free_function_declaration(c_ast_function_declaration *declaration);
void c_ast_free_variable(c_ast_variable *variable);

void c_parser_synchronize_to_declaration(c_parser *parser);
void c_parser_synchronize(c_parser *parser);

#endif  // !PARSER_H
