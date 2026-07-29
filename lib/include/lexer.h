#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>
#include "xi_string.h"

typedef enum {
  C_IDENTIFIER = 0,

  C_INTEGER_LITERAL,
  C_STRING_LITERAL,
  C_CHAR_LITERAL,

  C_INTEGER,
  C_CHAR,
  C_VOID,

  C_RETURN,
  C_IF,
  C_ELSE,
  C_EXTERN,
  C_TYPEDEF,
  C_UNSIGNED,
  C_SIGNED,
  C_SHORT,
  C_LONG,
  C_STRUCT,
  C_UNION,
  C_ENUM,
  C_STATIC,
  C_CONST,
  C_VOLATILE,

  C_PLUS,
  C_MINUS,
  C_ASTERISK,
  C_SLASH,

  C_LPAREN,
  C_RPAREN,
  C_LBRACE,
  C_RBRACE,
  C_LBRACKET,
  C_RBRACKET,
  C_SEMICOLON,
  C_ASSIGN,
  C_AMPERSAND,

  C_LESS,
  C_GREATER,
  C_LESS_EQUAL,
  C_GREATER_EQUAL,
  C_EQUAL,
  C_NOT_EQUAL,
  C_PIPE,
  C_PIPE_PIPE,
  C_COLON,
  C_COMMA,
  C_PERCENT,
  C_TILDE,
  C_CARET,
  C_DOT,
  C_QUESTION,
  C_ARROW,

  C_EOF,
} C_TokenType;

typedef struct {
  C_TokenType type;

  // NOTE: removed union, because otherwise
  // cannot set string to NULL (causes double free)
  String string;
  char   symbol;

  int line;
  int column;
} C_Token;

typedef struct {
  const char *source;
  size_t      source_length;
  char        current_char;
  size_t      current_position;
  size_t      read_position;

  size_t current_line;
  size_t current_column;
  size_t start_line;
  size_t start_column;
} C_Lexer;

C_Lexer *C_LexerCreate(const char *source);

void        C_LexerStartToken(C_Lexer *lexer);
C_Token     C_LexerCreateToken(C_Lexer *lexer, C_TokenType type, String string,
                               char symbol);
C_Token    *C_LexerLex(C_Lexer *lexer);
void        C_LexerFree(C_Lexer *lexer);
void        C_LexerFreeTokens(C_Token *tokens);
const char *C_TokenTypeToString(C_TokenType type);

#endif  // LEXER_H
