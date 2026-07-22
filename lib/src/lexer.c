#include "lexer.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stb_ds.h"
#include "utils.h"
#include "xi_string.h"

void C_LexerAdvance(C_Lexer *lexer) {
  if (lexer->current_position == lexer->source_length) {
    EXIT_WITH_ERROR(
        "Already reached the end of the source, line: %zu, columns: %zu\n",
        lexer->current_line, lexer->current_position);
  }

  if (lexer->current_char == '\n') {
    lexer->current_line++;
    lexer->current_column = 1;
  } else {
    lexer->current_column++;
  }

  lexer->current_char     = lexer->source[lexer->read_position];
  lexer->current_position = lexer->read_position;
  lexer->read_position++;
}

void C_LexerStartToken(C_Lexer *lexer) {
  lexer->start_line   = lexer->current_line;
  lexer->start_column = lexer->current_column;
}

C_Token C_LexerCreateToken(C_Lexer *lexer, C_TokenType type, String string,
                           char symbol) {
  C_Token token = {.type   = type,
                   .string = string,
                   .symbol = symbol,
                   .line   = lexer->start_line,
                   .column = lexer->start_column};
  LOG_DEBUG("Created token %s\n", C_TokenTypeToString(token.type));
  return token;
}

int C_IsWhitespace(char character) {
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

void C_LexerSkipWhitespaces(C_Lexer *lexer) {
  while (C_IsWhitespace(lexer->current_char)) {
    LOG_DEBUG("Skipped whitespace: %d\n", lexer->current_char);
    C_LexerAdvance(lexer);
  }
}

C_Lexer *C_LexerCreate(const char *source) {
  if (!source) {
    EXIT_WITH_ERROR("Provided empty source, nothing to parse!");
  }

  C_Lexer *lexer = malloc(sizeof(C_Lexer));

  if (!lexer) {
    EXIT_WITH_ERROR("Failed to allocate memory for lexer");
  }

  lexer->source           = source;
  lexer->source_length    = strlen(source);
  lexer->current_char     = source[0];
  lexer->current_position = 0;
  lexer->read_position    = 1;

  lexer->current_line   = 1;
  lexer->current_column = 1;
  lexer->start_line     = 1;
  lexer->start_column   = 1;

  return lexer;
}

String C_AllocateSubstring(const char *source, size_t start_position,
                           size_t end_position) {
  assert(start_position <= end_position);

  String s = StringCreateEmpty(end_position - start_position + 1);

  for (size_t i = start_position; i < end_position; i++) {
    StringAppendChar(&s, source[i]);
  }

  LOG_DEBUG("Substring: %s, Length: %zu\n", StringGetCstr(&s), s.length);

  return s;
}

C_Token C_LexerLexNumber(C_Lexer *lexer) {
  size_t start_position = lexer->current_position;

  LOG_DEBUG("Lexing number\n");
  LOG_DEBUG("Current position %zu\n", lexer->current_position);

  while (isdigit(lexer->current_char)) {
    LOG_DEBUG("%c\n", lexer->current_char);
    C_LexerAdvance(lexer);
  }

  size_t end_position = lexer->current_position;
  String number =
      C_AllocateSubstring(lexer->source, start_position, end_position);

  return C_LexerCreateToken(lexer, C_INTEGER_LITERAL, number, '\0');
}

C_Token C_LexerLexIdentifierOrKeyword(C_Lexer *lexer) {
  size_t start_position = lexer->current_position;

  LOG_DEBUG("Lexing identifier or keyword\n");
  LOG_DEBUG("Current position %zu\n", lexer->current_position);

  while (isalnum(lexer->current_char)) {
    LOG_DEBUG("%c\n", lexer->current_char);
    C_LexerAdvance(lexer);
  }

  size_t end_position = lexer->current_position;
  String word =
      C_AllocateSubstring(lexer->source, start_position, end_position);

  C_TokenType type      = C_IDENTIFIER;
  const char *word_cstr = StringGetCstr(&word);
  if (strcmp("int", word_cstr) == 0) {
    type = C_INTEGER;
  } else if (strcmp("void", word_cstr) == 0) {
    type = C_VOID;
  } else if (strcmp("return", word_cstr) == 0) {
    type = C_RETURN;
  }

  return C_LexerCreateToken(lexer, type, word, '\0');
}

C_Token *C_LexerLex(C_Lexer *lexer) {
  LOG_DEBUG("Start parsing, current position: %zu\n", lexer->current_position);

  C_Token *tokens = NULL;

  while (lexer->current_position < lexer->source_length) {
    C_LexerSkipWhitespaces(lexer);

    C_LexerStartToken(lexer);

    switch (lexer->current_char) {
      case '\0':
        arrput(tokens, C_LexerCreateToken(lexer, C_EOF, (String){0},
                                          lexer->current_char));
        break;

      case '+':
        arrput(tokens, C_LexerCreateToken(lexer, C_PLUS, (String){0},
                                          lexer->current_char));
        C_LexerAdvance(lexer);
        break;

      case '-':
        arrput(tokens, C_LexerCreateToken(lexer, C_MINUS, (String){0},
                                          lexer->current_char));
        C_LexerAdvance(lexer);
        break;

      case '*':
        arrput(tokens, C_LexerCreateToken(lexer, C_ASTERISK, (String){0},
                                          lexer->current_char));
        C_LexerAdvance(lexer);
        break;

      case '/':
        arrput(tokens, C_LexerCreateToken(lexer, C_SLASH, (String){0},
                                          lexer->current_char));
        C_LexerAdvance(lexer);
        break;

      case '{':
        arrput(tokens, C_LexerCreateToken(lexer, C_LBRACE, (String){0},
                                          lexer->current_char));
        C_LexerAdvance(lexer);
        break;
      case '}':
        arrput(tokens, C_LexerCreateToken(lexer, C_RBRACE, (String){0},
                                          lexer->current_char));
        C_LexerAdvance(lexer);
        break;
      case '(':
        arrput(tokens, C_LexerCreateToken(lexer, C_LPAREN, (String){0},
                                          lexer->current_char));
        C_LexerAdvance(lexer);
        break;
      case ')':
        arrput(tokens, C_LexerCreateToken(lexer, C_RPAREN, (String){0},
                                          lexer->current_char));
        C_LexerAdvance(lexer);
        break;

      case ';':
        arrput(tokens, C_LexerCreateToken(lexer, C_SEMICOLON, (String){0},
                                          lexer->current_char));
        C_LexerAdvance(lexer);
        break;
      case '=':
        arrput(tokens, C_LexerCreateToken(lexer, C_ASSIGN, (String){0},
                                          lexer->current_char));
        C_LexerAdvance(lexer);
        break;
      default: {
        if (isdigit(lexer->current_char)) {
          arrput(tokens, C_LexerLexNumber(lexer));
          continue;
        }

        if (isalpha(lexer->current_char)) {
          arrput(tokens, C_LexerLexIdentifierOrKeyword(lexer));
          continue;
        }

        EXIT_WITH_ERROR(
            "Got unknown character: \'%c\' (%d), position: %zu, line: %zu, "
            "column: %zu\n",
            lexer->current_char, lexer->current_char, lexer->current_position,
            lexer->current_line, lexer->current_column);
      }
    }
  }

  return tokens;
}

void C_LexerFree(C_Lexer *lexer) {
  free(lexer);
  lexer = NULL;
}

void C_LexerFreeTokens(C_Token *tokens) {
  if (tokens == NULL) {
    return;
  }

  for (int i = 0; i < arrlen(tokens); i++) {
    StringFree(&tokens[i].string);
  }

  arrfree(tokens);
  tokens = NULL;
}

const char *C_TokenTypeToString(C_TokenType type) {
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
