#include "parser.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "utils.h"
#include "stb_ds.h"

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
    if (parser->current_position == arrlenu(parser->tokens)) {
        EXIT_WITH_ERROR("Already reached the end of the source\n");
    }

    parser->current_token = parser->tokens[parser->read_position];
    parser->current_position = parser->read_position;
    parser->read_position++;
}

c_token c_parser_peek(c_parser *parser) {
    if (parser->current_position == arrlenu(parser->tokens)) {
        EXIT_WITH_ERROR("Trying to read after the end of tokens");
    }

    return parser->tokens[parser->read_position];
}

c_ast_constant *c_parser_parse_constant(c_parser *parser) {
    c_ast_constant *constant = malloc(sizeof(c_ast_constant));

    // NOTE: returns 0 if invalid, maybe change to some other function later
    constant->value = atoi(parser->current_token.string);

    c_parser_advance(parser);

    return constant;
}

c_ast_expression *c_parser_parse_expression(c_parser *parser) {
    c_ast_expression *expression = malloc(sizeof(c_ast_expression));

    expression->constant = NULL;

    switch (parser->current_token.type) {
        // TODO: currently only integer
        case C_INTEGER_LITERAL:
            expression->constant = c_parser_parse_constant(parser);
            expression->type = C_CONSTANT;
            break;
        default:
            EXIT_WITH_ERROR_ARGS(
                "Received improper token for parse expression: %d\n",
                parser->current_token.type);
    }

    return expression;
}

c_ast_return *c_parser_parse_return(c_parser *parser) {
    c_ast_return *return_statement = malloc(sizeof(c_ast_return));

    assert(parser->current_token.type == C_RETURN);
    c_parser_advance(parser);

    return_statement->value = c_parser_parse_expression(parser);

    return return_statement;
}

c_ast_block *c_parser_parse_block(c_parser *parser) {
    c_ast_block *block = malloc(sizeof(c_ast_block));

    block->statements = NULL;

    assert(parser->current_token.type == C_LBRACE);
    c_parser_advance(parser);

    // TODO: for now parse only return (later use parse_statement)
    c_ast_return *return_statement = c_parser_parse_return(parser);

    c_ast_statement *statement = malloc(sizeof(c_ast_statement));

    statement->type = C_STATEMENT_RETURN;
    statement->return_statement = return_statement;

    arrput(block->statements, statement);

    assert(c_parser_peek(parser).type == C_RBRACE);
    c_parser_advance(parser);

    return block;
}

c_ast_function_declaration *c_parser_parse_function_declaration(
    c_parser *parser) {
    // NOTE: using asserts is not great :)
    c_ast_function_declaration *function_declaration =
        malloc(sizeof(c_ast_function_declaration));

    function_declaration->body = NULL;

    c_parser_advance(parser);
    assert(parser->current_token.type == C_IDENTIFIER);

    long length = strlen(parser->current_token.string);
    function_declaration->function_name = malloc(sizeof(char) * (length + 1));
    strcpy(function_declaration->function_name, parser->current_token.string);

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

    program->function_declaration = NULL;

    switch (parser->current_token.type) {
        case C_INTEGER:
            // NOTE: temporary main function parse
            program->function_declaration =
                c_parser_parse_function_declaration(parser);
            break;

        case C_EOF:
            break;
        default:
            EXIT_WITH_ERROR_ARGS("Received improper token for parse: %d\n",
                                 parser->current_token.type);
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
        default:
            EXIT_WITH_ERROR("Got unknown expression to free\n");
    }

    free(expression);
}

void c_ast_free_statement_block(c_ast_block *block) {
    if (!block) {
        return;
    }

    for (int i = 0; i < arrlen(block->statements); i++) {
        c_ast_free_statement(block->statements[i]);
    }

    arrfree(block->statements);
    free(block);
}

void c_ast_free_statement_return(c_ast_return *ret) {
    if (!ret) {
        return;
    }

    c_ast_free_expression(ret->value);
    free(ret);
}

void c_ast_free_function_declaration(c_ast_function_declaration *declaration) {
    if (!declaration) {
        return;
    }

    free(declaration->function_name);
    c_ast_free_statement_block(declaration->body);
    free(declaration);
}

void c_ast_free_statement(c_ast_statement *statement) {
    if (!statement) {
        return;
    }

    switch (statement->type) {
        case C_STATEMENT_BLOCK:
            c_ast_free_statement_block(statement->block);
            break;
        case C_STATEMENT_RETURN:
            c_ast_free_statement_return(statement->return_statement);
            break;
        case C_STATEMENT_FUNCTION_DECLARATION:
            c_ast_free_function_declaration(statement->function_declaration);
            break;
        default:
            EXIT_WITH_ERROR("Got unknown statement to free\n");
    }

    free(statement);
}

void c_parser_free_program(c_ast_program *program) {
    if (!program) {
        return;
    }

    if (program->function_declaration) {
        c_ast_free_function_declaration(program->function_declaration);
    }
    free(program);
}
