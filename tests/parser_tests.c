#include "lexer.h"
#include "parser.h"
#include "stb_ds.h"
#include "unity.h"
#include "xi_string.h"

void setUp(void) {}

void tearDown(void) {}

void test_parse_function_declaration(void) {
  const char source[1024] =
      "int main() {"
      "   return 0;"
      "}";

  C_ErrorContext *error_context = C_ErrorContextCreate();

  if (!error_context) {
    fprintf(stderr, "Failed to allocate memory for error_context\n");
    return;
  }

  C_Lexer  *lexer  = C_LexerCreate(source);
  C_Token  *tokens = C_LexerLex(lexer);
  C_Parser *parser = C_ParserCreate(tokens, error_context, "test_filename.c");

  C_AstFunctionDeclaration *func = C_ParserParseFunctionDeclaration(parser);

  TEST_ASSERT_NOT_NULL(func);
  TEST_ASSERT_EQUAL_STRING("main", StringGetCstr(&func->function_name));
  TEST_ASSERT_NOT_NULL(func->body);
  TEST_ASSERT_EQUAL(1, arrlen(func->body->statements));

  C_AstStatement *stmt = func->body->statements[0];
  TEST_ASSERT_EQUAL(C_STATEMENT_RETURN, stmt->type);
  TEST_ASSERT_NOT_NULL(stmt->return_statement);
  TEST_ASSERT_EQUAL(0, stmt->return_statement->value->constant->value.int_value);

  C_AstFreeFunctionDeclaration(func);
  C_ParserFree(parser);
  C_ErrorContextFree(error_context);
  C_LexerFree(lexer);
}

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  UNITY_BEGIN();

  RUN_TEST(test_parse_function_declaration);
  return UNITY_END();
}
