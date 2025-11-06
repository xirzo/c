#include "lexer.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stb_ds.h"
#include "utils.h"

void c_lexer_advance(c_lexer *lexer) {
    if (lexer->current_position == lexer->source_length) {
        EXIT_WITH_ERROR(
            "Already reached the end of the source, line: %zu, columns: %zu\n",
            lexer->current_line,
            lexer->current_position);
    }

    if (lexer->current_char == '\n') {
        lexer->current_line++;
        lexer->current_column = 1;
    } else {
        lexer->current_column++;
    }

    lexer->current_char = lexer->source[lexer->read_position];
    lexer->current_position = lexer->read_position;
    lexer->read_position++;
}

void c_lexer_start_token(c_lexer *lexer) {
    lexer->start_line = lexer->current_line;
    lexer->start_column = lexer->current_column;
}

c_token c_lexer_create_token(c_lexer *lexer,
                             c_token_type type,
                             char *string,
                             char symbol) {
    c_token token = {.type = type,
                     .string = string,
                     .symbol = symbol,
                     .line = lexer->start_line,
                     .column = lexer->start_column};
    LOG_DEBUG("Created token %s\n", c_token_type_to_string(token.type));
    return token;
}

int is_whitespace(char character) {
    switch (character) {
        case '\t':
        case '\n':
        case '\v':
        case '\f':
        case '\r':
        case ' ':
            return 1;
        default:
            return 0;
    }
}

void c_lexer_skip_whitespaces(c_lexer *lexer) {
    while (is_whitespace(lexer->current_char)) {
        LOG_DEBUG("Skipped whitespace: %d\n", lexer->current_char);
        c_lexer_advance(lexer);
    }
}

c_lexer *c_lexer_create(const char *source) {
    if (!source) {
        EXIT_WITH_ERROR("Provided empty source, nothing to parse!");
    }

    c_lexer *lexer = malloc(sizeof(c_lexer));

    if (!lexer) {
        EXIT_WITH_ERROR("Failed to allocate memory for lexer");
    }

    lexer->source = source;
    lexer->source_length = strlen(source);
    lexer->current_char = source[0];
    lexer->current_position = 0;
    lexer->read_position = 1;

    lexer->current_line = 1;
    lexer->current_column = 1;
    lexer->start_line = 1;
    lexer->start_column = 1;

    return lexer;
}

char *allocate_substring(const char *source,
                         size_t start_position,
                         size_t end_position) {
    assert(start_position <= end_position);

    size_t length = end_position - start_position + 1;

    char *string = malloc(sizeof(char) * length);

    if (!string) {
        EXIT_WITH_ERROR("Failed to allocate memory for substring");
    }

    strncpy(string, source + start_position, length);

    string[length - 1] = '\0';

    LOG_DEBUG("Substring: %s, Length: %zu\n", string, length);

    return string;
}

c_token c_lexer_lex_number(c_lexer *lexer) {
    size_t start_position = lexer->current_position;

    LOG_DEBUG("Lexing number\n");
    LOG_DEBUG("Current position %zu\n", lexer->current_position);

    while (isdigit(lexer->current_char)) {
        LOG_DEBUG("%c\n", lexer->current_char);
        c_lexer_advance(lexer);
    }

    size_t end_position = lexer->current_position;
    char *number =
        allocate_substring(lexer->source, start_position, end_position);

    return c_lexer_create_token(lexer, C_INTEGER_LITERAL, number, '\0');
}

c_token c_lexer_lex_identifier_or_keyword(c_lexer *lexer) {
    size_t start_position = lexer->current_position;

    LOG_DEBUG("Lexing identifier or keyword\n");
    LOG_DEBUG("Current position %zu\n", lexer->current_position);

    while (isalnum(lexer->current_char)) {
        LOG_DEBUG("%c\n", lexer->current_char);
        c_lexer_advance(lexer);
    }

    size_t end_position = lexer->current_position;
    char *word =
        allocate_substring(lexer->source, start_position, end_position);

    c_token_type type = C_IDENTIFIER;
    if (strcmp("int", word) == 0) {
        type = C_INTEGER;
    } else if (strcmp("void", word) == 0) {
        type = C_VOID;
    } else if (strcmp("return", word) == 0) {
        type = C_RETURN;
    }

    return c_lexer_create_token(lexer, type, word, '\0');
}

c_token *c_lexer_lex(c_lexer *lexer) {
    LOG_DEBUG("Start parsing, current position: %zu\n",
              lexer->current_position);

    c_token *tokens = NULL;

    while (lexer->current_position < lexer->source_length) {
        c_lexer_skip_whitespaces(lexer);

        c_lexer_start_token(lexer);

        switch (lexer->current_char) {
            case '\0':
                arrput(tokens,
                       c_lexer_create_token(
                           lexer, C_EOF, NULL, lexer->current_char));
                break;

            case '+':
                arrput(tokens,
                       c_lexer_create_token(
                           lexer, C_PLUS, NULL, lexer->current_char));
                c_lexer_advance(lexer);
                break;

            case '-':
                arrput(tokens,
                       c_lexer_create_token(
                           lexer, C_MINUS, NULL, lexer->current_char));
                c_lexer_advance(lexer);
                break;

            case '*':
                arrput(tokens,
                       c_lexer_create_token(
                           lexer, C_ASTERISK, NULL, lexer->current_char));
                c_lexer_advance(lexer);
                break;

            case '/':
                arrput(tokens,
                       c_lexer_create_token(
                           lexer, C_SLASH, NULL, lexer->current_char));
                c_lexer_advance(lexer);
                break;

            case '{':
                arrput(tokens,
                       c_lexer_create_token(
                           lexer, C_LBRACE, NULL, lexer->current_char));
                c_lexer_advance(lexer);
                break;
            case '}':
                arrput(tokens,
                       c_lexer_create_token(
                           lexer, C_RBRACE, NULL, lexer->current_char));
                c_lexer_advance(lexer);
                break;
            case '(':
                arrput(tokens,
                       c_lexer_create_token(
                           lexer, C_LPAREN, NULL, lexer->current_char));
                c_lexer_advance(lexer);
                break;
            case ')':
                arrput(tokens,
                       c_lexer_create_token(
                           lexer, C_RPAREN, NULL, lexer->current_char));
                c_lexer_advance(lexer);
                break;

            case ';':
                arrput(tokens,
                       c_lexer_create_token(
                           lexer, C_SEMICOLON, NULL, lexer->current_char));
                c_lexer_advance(lexer);
                break;
            case '=':
                arrput(tokens,
                       c_lexer_create_token(
                           lexer, C_ASSIGN, NULL, lexer->current_char));
                c_lexer_advance(lexer);
                break;
            default: {
                if (isdigit(lexer->current_char)) {
                    arrput(tokens, c_lexer_lex_number(lexer));
                    continue;
                }

                if (isalpha(lexer->current_char)) {
                    arrput(tokens, c_lexer_lex_identifier_or_keyword(lexer));
                    continue;
                }

                EXIT_WITH_ERROR(
                    "Got unknown character: \'%c\' (%d), position: %zu, line: %zu, column: %zu\n",
                    lexer->current_char,
                    lexer->current_char,
                    lexer->current_position,
                    lexer->current_line,
                    lexer->current_column);
            }
        }
    }

    return tokens;
}

void c_lexer_free(c_lexer *lexer) {
    free(lexer);
    lexer = NULL;
}

void c_lexer_free_tokens(c_token *tokens) {
    if (tokens == NULL) {
        return;
    }

    for (int i = 0; i < arrlen(tokens); i++) {
        if (tokens[i].string) {
            free(tokens[i].string);
            tokens[i].string = NULL;
        }
    }

    arrfree(tokens);
    tokens = NULL;
}

const char *c_token_type_to_string(c_token_type type) {
    switch (type) {
        case C_IDENTIFIER:
            return "C_IDENTIFIER";
        case C_INTEGER_LITERAL:
            return "C_INTEGER_LITERAL";
        case C_INTEGER:
            return "C_INTEGER";
        case C_RETURN:
            return "C_RETURN";
        case C_PLUS:
            return "C_PLUS";
        case C_MINUS:
            return "C_MINUS";
        case C_ASTERISK:
            return "C_ASTERISK";
        case C_SLASH:
            return "C_SLASH";
        case C_LPAREN:
            return "C_LPAREN";
        case C_RPAREN:
            return "C_RPAREN";
        case C_LBRACE:
            return "C_LBRACE";
        case C_RBRACE:
            return "C_RBRACE";
        case C_SEMICOLON:
            return "C_SEMICOLON";
        case C_ASSIGN:
            return "C_ASSIGN";
        case C_VOID:
            return "C_VOID";
        case C_EOF:
            return "C_EOF";
        default:
            assert("NOT EXISTING TOKEN");
    }

    return NULL;
}
