#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

typedef struct c_ast_expression c_ast_expression;
typedef struct c_ast_statement c_ast_statement;

typedef enum {
    // NOTE: maybe should separate
    // into different types of literals
    C_CONSTANT,
    C_FUNCTION_CALL,
} c_ast_expression_type;

typedef struct {
    // NOTE: for now only int
    int value;
} c_ast_constant;

typedef struct {
    char *function_name;
} c_ast_function_call;

typedef struct c_ast_expression {
    c_ast_expression_type type;

    union {
        c_ast_constant *constant;
        c_ast_function_call *function_call;
    };
} c_ast_expression;

typedef enum {
    C_STATEMENT_BLOCK,
    C_STATEMENT_RETURN,
    C_STATEMENT_FUNCTION_DECLARATION,
} c_ast_statement_type;

typedef struct {
    c_ast_statement **statements;
} c_ast_block;

typedef struct {
    c_ast_expression *value;
} c_ast_return;

typedef struct {
    // TODO: add return type
    char *function_name;
    c_ast_block *body;
} c_ast_function_declaration;

typedef struct c_ast_statement {
    c_ast_statement_type type;

    union {
        c_ast_block *block;
        c_ast_return *return_statement;
        c_ast_function_declaration *function_declaration;
    };
} c_ast_statement;

typedef struct {
    c_ast_function_declaration **function_declarations;
} c_ast_program;

typedef struct {
    c_token *tokens;
    c_token current_token;
    size_t current_position;
    size_t read_position;
} c_parser;

c_parser *c_parser_create(c_token *tokens);
c_ast_program *c_parser_parse(c_parser *parser);
void c_parser_free(c_parser *parser);
void c_parser_free_program(c_ast_program *program);

// NOTE: in fact not a part of public api, but can be used
void c_parser_advance(c_parser *parser);
c_token c_parser_peek(c_parser *parser);

c_ast_expression *c_parser_parse_expression(c_parser *parser);
c_ast_constant *c_parser_parse_constant(c_parser *parser);
c_ast_function_call *c_parser_parse_function_call(c_parser *parser);
c_ast_return *c_parser_parse_return(c_parser *parser);
c_ast_block *c_parser_parse_block(c_parser *parser);
c_ast_function_declaration *c_parser_parse_function_declaration(
    c_parser *parser);

void c_ast_free_expression(c_ast_expression *expression);
void c_ast_free_statement(c_ast_statement *statement);
void c_ast_free_statement_block(c_ast_block *block);
void c_ast_free_statement_return(c_ast_return *ret);
void c_ast_free_function_declaration(c_ast_function_declaration *declaration);

#endif  // !PARSER_H
