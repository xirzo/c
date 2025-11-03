#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

typedef enum {
    C_IDENTIFIER = 0,

    C_INTEGER_LITERAL,

    C_INTEGER,
    C_RETURN,

    C_LPAREN,
    C_RPAREN,
    C_LBRACE,
    C_RBRACE,
    C_SEMICOLON,
    C_ASSIGN,
    C_EOF,
} c_token_type;

typedef struct {
    c_token_type type;

    // NOTE: removed union, because otherwise
    // cannot set string to NULL (causes double free)
    char *string;
    char symbol;
} c_token;

typedef struct {
    const char *source;
    size_t source_length;
    char current_char;
    size_t current_position;
    size_t read_position;
} c_lexer;

c_lexer *c_lexer_create(const char *source);
c_token *c_lexer_lex(c_lexer *lexer);
void c_lexer_free(c_lexer *lexer);
void c_lexer_free_tokens(c_token *tokens);

#endif  // LEXER_H
