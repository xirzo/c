#include "parser.h"
#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include "error.h"
#include "lexer.h"
#include "utils.h"
#include "stb_ds.h"
#include "str.h"

c_parser *c_parser_create(c_token *tokens,
                          c_error_context *error_context,
                          const char *filename) {
    if (arrlen(tokens) == 0) {
        c_error_report(error_context, "Got empty token list", filename, 1, 1);
        return NULL;
    }

    c_parser *parser = malloc(sizeof(c_parser));

    parser->current_position = 0;
    parser->read_position = 1;
    parser->current_token = tokens[0];
    parser->tokens = tokens;
    parser->error_context = error_context;
    parser->filename = filename;

    return parser;
}

void c_parser_advance(c_parser *parser) {
    size_t tokens_len = arrlenu(parser->tokens);

    if (parser->read_position >= tokens_len) {
        parser->current_token.type = C_EOF;
        parser->current_position = tokens_len;
        return;
    }

    parser->current_token = parser->tokens[parser->read_position];
    parser->current_position = parser->read_position;
    parser->read_position++;
}

c_token c_parser_peek(c_parser *parser) {
    size_t tokens_len = arrlenu(parser->tokens);

    if (parser->read_position >= tokens_len) {
        c_token eof_token = {0};
        eof_token.type = C_EOF;
        return eof_token;
    }

    return parser->tokens[parser->read_position];
}

c_token c_parser_peek_ahead(c_parser *parser) {
    size_t tokens_len = arrlenu(parser->tokens);

    if (parser->read_position + 1 >= tokens_len) {
        c_token eof_token = {0};
        eof_token.type = C_EOF;
        return eof_token;
    }

    return parser->tokens[parser->read_position + 1];
}

c_ast_constant *c_parser_parse_constant(c_parser *parser) {
    c_ast_constant *constant = malloc(sizeof(c_ast_constant));
    constant->value = atoi(parser->current_token.string);
    c_parser_advance(parser);
    return constant;
}

c_ast_function_call *c_parser_parse_function_call(c_parser *parser) {
    c_ast_function_call *function_call = malloc(sizeof(c_ast_function_call));
    function_call->function_name = strdup(parser->current_token.string);

    LOG_DEBUG("Parsing function call\n");
    c_parser_advance(parser);

    if (parser->current_token.type != C_LPAREN) {
        c_error_report_with_token(parser->error_context,
                                  "Expected '(' after function name",
                                  parser->current_token,
                                  parser->filename);
        free(function_call->function_name);
        free(function_call);
        return NULL;
    }

    c_parser_advance(parser);

    if (parser->current_token.type != C_RPAREN) {
        c_error_report_with_token(parser->error_context,
                                  "Expected ')' after function call",
                                  parser->current_token,
                                  parser->filename);
        free(function_call->function_name);
        free(function_call);
        return NULL;
    }

    c_parser_advance(parser);
    return function_call;
}

c_ast_statement *c_parser_parse_statement(c_parser *parser) {
    c_ast_statement *statement = malloc(sizeof(c_ast_statement));

    LOG_DEBUG("Parsing statement\n");

    switch (parser->current_token.type) {
        case C_INTEGER:
            if (c_parser_peek_ahead(parser).type == C_LPAREN) {
                statement->type = C_STATEMENT_FUNCTION_DECLARATION;
                statement->function_declaration =
                    c_parser_parse_function_declaration(parser);
                if (!statement->function_declaration) {
                    free(statement);
                    return NULL;
                }
                break;
            }

            statement->type = C_STATEMENT_ASSIGNMENT;
            statement->assignment = c_parser_parse_variable_assignment(parser);
            if (!statement->assignment) {
                free(statement);
                return NULL;
            }
            break;
        case C_LBRACE:
            statement->type = C_STATEMENT_BLOCK;
            statement->block = c_parser_parse_block(parser);
            if (!statement->block) {
                free(statement);
                return NULL;
            }
            break;
        case C_RETURN:
            statement->type = C_STATEMENT_RETURN;
            statement->return_statement = c_parser_parse_return(parser);
            if (!statement->return_statement) {
                free(statement);
                return NULL;
            }
            break;
        case C_SEMICOLON:
            statement->type = C_STATEMENT_NOOP;
            c_parser_advance(parser);
            break;
        default:
            statement->type = C_STATEMENT_EXPRESSION;
            statement->expression = c_parser_parse_expression(parser);
            if (!statement->expression) {
                free(statement);
                return NULL;
            }

            if (parser->current_token.type != C_SEMICOLON) {
                c_error_report_with_token(parser->error_context,
                                          "Expected ';' after expression",
                                          parser->current_token,
                                          parser->filename);
                c_ast_free_expression(statement->expression);
                free(statement);
                return NULL;
            }
            c_parser_advance(parser);
            break;
    }

    return statement;
}

c_ast_variable_assignment *c_parser_parse_variable_assignment(
    c_parser *parser) {
    LOG_DEBUG("Parsing variable assignment\n");
    c_ast_variable_assignment *assignment =
        malloc(sizeof(c_ast_variable_assignment));

    c_parser_advance(parser);

    if (parser->current_token.type != C_IDENTIFIER) {
        c_error_report_with_token(parser->error_context,
                                  "Expected identifier after type",
                                  parser->current_token,
                                  parser->filename);
        free(assignment);
        return NULL;
    }

    assignment->variable_name = strdup(parser->current_token.string);

    c_parser_advance(parser);

    if (parser->current_token.type != C_ASSIGN) {
        c_error_report_with_token(parser->error_context,
                                  "Expected '=' after variable name",
                                  parser->current_token,
                                  parser->filename);
        free(assignment->variable_name);
        free(assignment);
        return NULL;
    }

    c_parser_advance(parser);

    assignment->expression = c_parser_parse_expression(parser);
    if (!assignment->expression) {
        free(assignment->variable_name);
        free(assignment);
        return NULL;
    }

    if (parser->current_token.type != C_SEMICOLON) {
        c_error_report_with_token(parser->error_context,
                                  "Expected ';' after assignment",
                                  parser->current_token,
                                  parser->filename);
        free(assignment->variable_name);
        c_ast_free_expression(assignment->expression);
        free(assignment);
        return NULL;
    }
    c_parser_advance(parser);

    return assignment;
}

c_ast_expression *c_parser_parse_expression(c_parser *parser) {
    return c_parser_parse_expression_with_precedence(parser, 0);
}

c_infix_binding_power c_get_infix_binding_power(c_token_type token_type) {
    switch (token_type) {
        case C_PLUS:
        case C_MINUS:
            return (c_infix_binding_power){.left = 1, .right = 2};
        case C_ASTERISK:
        case C_SLASH:
            return (c_infix_binding_power){.left = 3, .right = 4};
        default:
            return (c_infix_binding_power){.left = 0, .right = 0};
    }
}

c_ast_expression *c_parser_parse_expression_with_precedence(
    c_parser *parser,
    double min_binding_power) {
    LOG_DEBUG("Parsing expression (Pratt Parsing)\n");

    c_ast_expression *lhs = malloc(sizeof(c_ast_expression));

    switch (parser->current_token.type) {
        case C_INTEGER_LITERAL: {
            lhs->type = C_CONSTANT;
            lhs->constant = c_parser_parse_constant(parser);
            break;
        }

        case C_IDENTIFIER: {
            if (c_parser_peek(parser).type == C_LPAREN) {
                lhs->type = C_FUNCTION_CALL;
                lhs->function_call = c_parser_parse_function_call(parser);
                if (!lhs->function_call) {
                    free(lhs);
                    return NULL;
                }
                break;
            }

            lhs->type = C_VARIABLE;
            lhs->variable = c_parser_parse_variable(parser);
            break;
        }

        default:
            c_error_report_with_token(parser->error_context,
                                      "Expected expression",
                                      parser->current_token,
                                      parser->filename);
            free(lhs);
            return NULL;
    }

    while (1) {
        c_token_type operator_type = parser->current_token.type;

        switch (operator_type) {
            case C_PLUS:
            case C_MINUS:
            case C_ASTERISK:
            case C_SLASH:
                break;
            case C_EOF:
                return lhs;
            default:
                return lhs;
        }

        c_infix_binding_power power = c_get_infix_binding_power(operator_type);

        if (power.left < min_binding_power) {
            break;
        }

        c_parser_advance(parser);

        c_ast_expression *rhs =
            c_parser_parse_expression_with_precedence(parser, power.right);
        if (!rhs) {
            c_ast_free_expression(lhs);
            return NULL;
        }

        c_ast_expression *binary_expr = malloc(sizeof(c_ast_expression));
        binary_expr->type = C_BINARY_EXPRESSION;
        binary_expr->binary = malloc(sizeof(c_ast_binary_expression));

        switch (operator_type) {
            case C_PLUS:
                binary_expr->binary->symbol = '+';
                break;
            case C_MINUS:
                binary_expr->binary->symbol = '-';
                break;
            case C_ASTERISK:
                binary_expr->binary->symbol = '*';
                break;
            case C_SLASH:
                binary_expr->binary->symbol = '/';
                break;
            default:
                binary_expr->binary->symbol = '?';
                break;
        }

        binary_expr->binary->lhs = lhs;
        binary_expr->binary->rhs = rhs;
        lhs = binary_expr;
    }

    return lhs;
}

c_ast_variable *c_parser_parse_variable(c_parser *parser) {
    c_ast_variable *variable = malloc(sizeof(c_ast_variable));

    LOG_DEBUG("Parsing variable\n");

    if (parser->current_token.type != C_IDENTIFIER) {
        c_error_report_with_token(parser->error_context,
                                  "Expected identifier",
                                  parser->current_token,
                                  parser->filename);
        free(variable);
        return NULL;
    }

    variable->name = strdup(parser->current_token.string);
    c_parser_advance(parser);

    return variable;
}

c_ast_return *c_parser_parse_return(c_parser *parser) {
    c_ast_return *return_statement = malloc(sizeof(c_ast_return));

    LOG_DEBUG("Parsing return\n");

    if (parser->current_token.type != C_RETURN) {
        c_error_report_with_token(parser->error_context,
                                  "Expected 'return' keyword",
                                  parser->current_token,
                                  parser->filename);
        free(return_statement);
        return NULL;
    }

    c_parser_advance(parser);

    return_statement->value = c_parser_parse_expression(parser);
    if (!return_statement->value) {
        free(return_statement);
        return NULL;
    }

    if (parser->current_token.type != C_SEMICOLON) {
        c_error_report_with_token(parser->error_context,
                                  "Expected ';' after return statement",
                                  parser->current_token,
                                  parser->filename);
        c_ast_free_expression(return_statement->value);
        free(return_statement);
        return NULL;
    }
    c_parser_advance(parser);

    return return_statement;
}

c_ast_block *c_parser_parse_block(c_parser *parser) {
    c_ast_block *block = malloc(sizeof(c_ast_block));
    block->statements = NULL;

    LOG_DEBUG("Parsing block\n");

    if (parser->current_token.type != C_LBRACE) {
        c_error_report_with_token(parser->error_context,
                                  "Expected '{' to start block",
                                  parser->current_token,
                                  parser->filename);
        free(block);
        return NULL;
    }

    c_parser_advance(parser);

    while (parser->current_token.type != C_RBRACE
           && parser->current_token.type != C_EOF) {
        c_ast_statement *statement = c_parser_parse_statement(parser);
        if (statement) {
            arrput(block->statements, statement);
        } else {
            c_parser_synchronize(parser);
        }
    }

    if (parser->current_token.type != C_RBRACE) {
        c_error_report_with_token(parser->error_context,
                                  "Expected '}' to end block",
                                  parser->current_token,
                                  parser->filename);
    } else {
        c_parser_advance(parser);
    }

    return block;
}

c_ast_function_declaration *c_parser_parse_function_declaration(
    c_parser *parser) {
    LOG_DEBUG("Parsing function declaration\n");
    c_ast_function_declaration *function_declaration =
        malloc(sizeof(c_ast_function_declaration));
    function_declaration->body = NULL;

    c_parser_advance(parser);

    if (parser->current_token.type != C_IDENTIFIER) {
        c_error_report_with_token(parser->error_context,
                                  "Expected function name after type",
                                  parser->current_token,
                                  parser->filename);
        free(function_declaration);
        return NULL;
    }

    function_declaration->function_name = strdup(parser->current_token.string);

    c_parser_advance(parser);

    if (parser->current_token.type != C_LPAREN) {
        c_error_report_with_token(parser->error_context,
                                  "Expected '(' after function name",
                                  parser->current_token,
                                  parser->filename);
        free(function_declaration->function_name);
        free(function_declaration);
        return NULL;
    }

    c_parser_advance(parser);

    if (parser->current_token.type != C_RPAREN) {
        c_error_report_with_token(parser->error_context,
                                  "Expected ')' after function parameters",
                                  parser->current_token,
                                  parser->filename);
        free(function_declaration->function_name);
        free(function_declaration);
        return NULL;
    }

    c_parser_advance(parser);

    function_declaration->body = c_parser_parse_block(parser);
    if (!function_declaration->body) {
        free(function_declaration->function_name);
        free(function_declaration);
        return NULL;
    }

    return function_declaration;
}

c_ast_program *c_parser_parse(c_parser *parser) {
    c_ast_program *program = malloc(sizeof(c_ast_program));
    program->function_declarations = NULL;

    while (parser->current_token.type != C_EOF) {
        switch (parser->current_token.type) {
            case C_INTEGER: {
                c_ast_function_declaration *decl =
                    c_parser_parse_function_declaration(parser);
                if (decl) {
                    arrput(program->function_declarations, decl);
                } else {
                    c_parser_synchronize_to_declaration(parser);
                }
            } break;
            case C_EOF:
                goto done_parsing;
            default:
                c_error_report_with_token(
                    parser->error_context,
                    "Unexpected token - expected function declaration",
                    parser->current_token,
                    parser->filename);
                c_parser_advance(parser);
                break;
        }
    }

done_parsing:
    return program;
}

void c_parser_synchronize(c_parser *parser) {
    while (parser->current_token.type != C_EOF) {
        switch (parser->current_token.type) {
            case C_SEMICOLON:
            case C_RBRACE:
            case C_LBRACE:
            case C_RETURN:
            case C_INTEGER:
                return;
            default:
                c_parser_advance(parser);
                break;
        }
    }
}

void c_parser_synchronize_to_declaration(c_parser *parser) {
    while (parser->current_token.type != C_EOF) {
        if (parser->current_token.type == C_INTEGER
            && c_parser_peek(parser).type == C_IDENTIFIER
            && c_parser_peek_ahead(parser).type == C_LPAREN) {
            return;
        }

        c_parser_advance(parser);
    }
}

void c_parser_free(c_parser *parser) {
    if (!parser) {
        return;
    }

    c_lexer_free_tokens(parser->tokens);
    free(parser);
}

void c_ast_free_expression(c_ast_expression *expression) {
    if (!expression) {
        return;
    }

    switch (expression->type) {
        case C_CONSTANT:
            free(expression->constant);
            break;
        case C_FUNCTION_CALL:
            free(expression->function_call->function_name);
            free(expression->function_call);
            break;
        case C_BINARY_EXPRESSION:
            c_ast_free_expression(expression->binary->lhs);
            c_ast_free_expression(expression->binary->rhs);
            free(expression->binary);
            break;
        case C_VARIABLE:
            c_ast_free_variable(expression->variable);
            break;
        default:
            EXIT_WITH_ERROR("Got unknown expression to free: %d\n",
                            expression->type);
    }

    free(expression);
}

void c_ast_free_block(c_ast_block *block) {
    if (!block) {
        return;
    }

    for (int i = 0; i < arrlen(block->statements); i++) {
        c_ast_free_statement(block->statements[i]);
    }

    arrfree(block->statements);
    free(block);
}

void c_ast_free_return(c_ast_return *ret) {
    if (!ret) {
        return;
    }

    c_ast_free_expression(ret->value);
    free(ret);
}

void c_ast_free_variable_assignment(c_ast_variable_assignment *assignment) {
    if (!assignment) {
        return;
    }

    free(assignment->variable_name);
    if (assignment->expression) {
        c_ast_free_expression(assignment->expression);
    }
    free(assignment);
}

void c_ast_free_function_declaration(c_ast_function_declaration *declaration) {
    if (!declaration) {
        return;
    }

    free(declaration->function_name);
    c_ast_free_block(declaration->body);
    free(declaration);
}

void c_ast_free_expression_statement(c_ast_expression *expression) {
    if (!expression) {
        return;
    }

    c_ast_free_expression(expression);
}

void c_ast_free_statement(c_ast_statement *statement) {
    if (!statement) {
        return;
    }

    switch (statement->type) {
        case C_STATEMENT_BLOCK:
            c_ast_free_block(statement->block);
            break;
        case C_STATEMENT_RETURN:
            c_ast_free_return(statement->return_statement);
            break;
        case C_STATEMENT_FUNCTION_DECLARATION:
            c_ast_free_function_declaration(statement->function_declaration);
            break;
        case C_STATEMENT_EXPRESSION:
            c_ast_free_expression_statement(statement->expression);
            break;
        case C_STATEMENT_ASSIGNMENT:
            c_ast_free_variable_assignment(statement->assignment);
            break;
        default:
            EXIT_WITH_ERROR("Got unknown statement to free: %d\n",
                            statement->type);
    }

    free(statement);
}

void c_parser_free_program(c_ast_program *program) {
    if (!program) {
        return;
    }

    if (program->function_declarations) {
        for (int i = 0; i < arrlen(program->function_declarations); i++) {
            c_ast_free_function_declaration(program->function_declarations[i]);
        }

        arrfree(program->function_declarations);
    }

    free(program);
}

void c_ast_free_variable(c_ast_variable *variable) {
    if (!variable) {
        return;
    }

    free(variable->name);
    free(variable);
}
