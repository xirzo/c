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
        EXIT_WITH_ERROR("Already reached the end of the source\n");
    }
    lexer->current_char = lexer->source[lexer->read_position];
    lexer->current_position = lexer->read_position;
    lexer->read_position++;
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

    return (c_token){.type = C_INTEGER_LITERAL, .string = number};
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

    if (strcmp("int", word) == 0) {
        return (c_token){.type = C_INTEGER, .string = word};
    }
    if (strcmp("return", word) == 0) {
        return (c_token){.type = C_RETURN, .string = word};
    }

    return (c_token){.type = C_IDENTIFIER, .string = word};
}

c_token *c_lexer_lex(c_lexer *lexer) {
    LOG_DEBUG("Start parsing, current position: %zu\n",
              lexer->current_position);

    c_token *tokens = NULL;

    while (lexer->current_position < lexer->source_length) {
        c_lexer_skip_whitespaces(lexer);
        switch (lexer->current_char) {
            case '\0':
                arrput(
                    tokens,
                    ((c_token){.type = C_EOF, .symbol = lexer->current_char}));
                c_lexer_advance(lexer);
                break;

            case '{':
                arrput(tokens,
                       ((c_token){.type = C_LBRACE,
                                  .symbol = lexer->current_char}));
                c_lexer_advance(lexer);
                break;
            case '}':
                arrput(tokens,
                       ((c_token){.type = C_RBRACE,
                                  .symbol = lexer->current_char}));
                c_lexer_advance(lexer);
                break;
            case '(':
                arrput(tokens,
                       ((c_token){.type = C_LPAREN,
                                  .symbol = lexer->current_char}));
                c_lexer_advance(lexer);
                break;
            case ')':
                arrput(tokens,
                       ((c_token){.type = C_RPAREN,
                                  .symbol = lexer->current_char}));
                c_lexer_advance(lexer);
                break;

            case ';':
                arrput(tokens,
                       ((c_token){.type = C_SEMICOLON,
                                  .symbol = lexer->current_char}));
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

                EXIT_WITH_ERROR_ARGS(
                    "Got unknown character: \'%c\' (%d), position: %zu\n",
                    lexer->current_char,
                    lexer->current_char,
                    lexer->current_position);
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
