#include "parser.h"
#include <stdlib.h>
#include "lexer.h"
#include "utils.h"
#include "stb_ds.h"

c_parser *c_parser_create(c_token *tokens) {
    c_parser *parser = malloc(sizeof(c_parser));

    parser->tokens = tokens;

    return parser;
}

c_ast_program *c_parser_parse(c_parser *parser) {
    return NULL;
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

void c_ast_free_statement_block(c_ast_statement_block *block) {
    if (!block) {
        return;
    }

    for (int i = 0; i < arrlen(block->statements); i++) {
        c_ast_free_statement(block->statements[i]);
    }

    arrfree(block->statements);
    free(block);
}

void c_ast_free_statement_return(c_ast_statement_return *ret) {
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

    c_ast_free_function_declaration(program->function_declaration);
    free(program);
}
