#include "parser.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "utils.h"
#include "stb_ds.h"
#include "str.h"

c_parser *c_parser_create(c_token *tokens) {
    if (arrlen(tokens) == 0) {
        EXIT_WITH_ERROR("Got empty tokens list\n");
    }

    c_parser *parser = malloc(sizeof(c_parser));

    parser->current_position = 0;
    parser->read_position = 1;
    parser->current_token = tokens[0];
    parser->tokens = tokens;

    return parser;
}

void c_parser_advance(c_parser *parser) {
    if (parser->current_position >= arrlenu(parser->tokens)) {
        EXIT_WITH_ERROR("Already reached the end of the source\n");
    }

    parser->current_token = parser->tokens[parser->read_position];
    parser->current_position = parser->read_position;
    parser->read_position++;
}

c_token c_parser_peek(c_parser *parser) {
    if (parser->current_position >= arrlenu(parser->tokens)) {
        EXIT_WITH_ERROR("Trying to read after the end of tokens");
    }

    return parser->tokens[parser->read_position];
}

c_token c_parser_peek_ahead(c_parser *parser) {
    if (parser->current_position + 1 >= arrlenu(parser->tokens)) {
        EXIT_WITH_ERROR("Trying to read ahead after the end of tokens");
    }

    return parser->tokens[parser->read_position + 1];
}

c_ast_constant *c_parser_parse_constant(c_parser *parser) {
    c_ast_constant *constant = malloc(sizeof(c_ast_constant));

    // NOTE: returns 0 if invalid, maybe change to some other function later
    constant->value = atoi(parser->current_token.string);

    c_parser_advance(parser);

    return constant;
}

c_ast_function_call *c_parser_parse_function_call(c_parser *parser) {
    c_ast_function_call *function_call = malloc(sizeof(c_ast_function_call));
    function_call->function_name = strdup(parser->current_token.string);

    LOG_DEBUG("Parsing function call\n");
    c_parser_advance(parser);
    assert(parser->current_token.type == C_LPAREN);

    c_parser_advance(parser);
    assert(parser->current_token.type == C_RPAREN);

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
                break;
            }

            statement->type = C_STATEMENT_ASSIGNMENT;
            statement->assignment = c_parser_parse_variable_assignment(parser);
            break;
        case C_LBRACE:
            statement->type = C_STATEMENT_BLOCK;
            statement->block = c_parser_parse_block(parser);
            break;
        case C_RETURN:
            statement->type = C_STATEMENT_RETURN;
            statement->return_statement = c_parser_parse_return(parser);
            break;
        case C_SEMICOLON:
            statement->type = C_STATEMENT_NOOP;
            break;
        default:
            statement->type = C_STATEMENT_EXPRESSION;
            statement->expression = c_parser_parse_expression(parser);
            assert(parser->current_token.type == C_SEMICOLON);
            c_parser_advance(parser);
            break;
    }

    return statement;
}

c_ast_variable_assignment *c_parser_parse_variable_assignment(
    c_parser *parser) {
    c_ast_variable_assignment *assignment =
        malloc(sizeof(c_ast_variable_assignment));

    c_parser_advance(parser);
    assert(parser->current_token.type == C_IDENTIFIER);

    assignment->variable_name = strdup(parser->current_token.string);

    c_parser_advance(parser);
    assert(parser->current_token.type == C_ASSIGN);

    c_parser_advance(parser);

    assignment->expression = c_parser_parse_expression(parser);

    assert(parser->current_token.type == C_SEMICOLON);
    c_parser_advance(parser);

    return assignment;
}

c_ast_expression *c_parser_parse_expression(c_parser *parser) {
    c_ast_expression *expression = malloc(sizeof(c_ast_expression));

    LOG_DEBUG("Parsing expression\n");

    switch (parser->current_token.type) {
        case C_INTEGER_LITERAL:
            expression->type = C_CONSTANT;
            expression->constant = c_parser_parse_constant(parser);
            break;

        case C_IDENTIFIER:
            expression->type = C_FUNCTION_CALL;
            expression->function_call = c_parser_parse_function_call(parser);
            break;

        default:
            free(expression);
            EXIT_WITH_ERROR(
                "Received improper token for parse expression: %d\n",
                parser->current_token.type);
    }

    return expression;
}

c_ast_return *c_parser_parse_return(c_parser *parser) {
    c_ast_return *return_statement = malloc(sizeof(c_ast_return));

    LOG_DEBUG("Parsing return\n");

    assert(parser->current_token.type == C_RETURN);
    c_parser_advance(parser);

    return_statement->value = c_parser_parse_expression(parser);

    assert(parser->current_token.type == C_SEMICOLON);
    c_parser_advance(parser);

    return return_statement;
}

c_ast_block *c_parser_parse_block(c_parser *parser) {
    c_ast_block *block = malloc(sizeof(c_ast_block));

    LOG_DEBUG("Parsing block\n");

    block->statements = NULL;

    assert(parser->current_token.type == C_LBRACE);
    c_parser_advance(parser);

    while (parser->current_token.type != C_RBRACE) {
        c_ast_statement *statement = c_parser_parse_statement(parser);
        arrput(block->statements, statement);
    }

    assert(parser->current_token.type == C_RBRACE);
    c_parser_advance(parser);

    return block;
}

c_ast_function_declaration *c_parser_parse_function_declaration(
    c_parser *parser) {
    // NOTE: using asserts is not great :)
    LOG_DEBUG("Parsing function declaration\n");
    c_ast_function_declaration *function_declaration =
        malloc(sizeof(c_ast_function_declaration));

    function_declaration->body = NULL;

    c_parser_advance(parser);
    assert(parser->current_token.type == C_IDENTIFIER);

    function_declaration->function_name = strdup(parser->current_token.string);

    c_parser_advance(parser);
    assert(parser->current_token.type == C_LPAREN);

    c_parser_advance(parser);
    assert(parser->current_token.type == C_RPAREN);

    c_parser_advance(parser);

    function_declaration->body = c_parser_parse_block(parser);

    return function_declaration;
}

c_ast_program *c_parser_parse(c_parser *parser) {
    c_ast_program *program = malloc(sizeof(c_ast_program));

    program->function_declarations = NULL;
    size_t tokens_len = arrlen(parser->tokens);

    while (parser->current_token.type != C_EOF
           && parser->current_position < tokens_len) {
        switch (parser->current_token.type) {
            case C_INTEGER:
                arrput(program->function_declarations,
                       c_parser_parse_function_declaration(parser));
                continue;
            case C_EOF:
                break;
            default:
                c_parser_free_program(program);
                EXIT_WITH_ERROR("Received improper token for parse: %d\n",
                                parser->current_token.type);
        }
    }

    return program;
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
