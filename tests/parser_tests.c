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
  TEST_ASSERT_EQUAL(0,
                    stmt->return_statement->value->constant->value.int_value);

  C_AstFreeFunctionDeclaration(func);
  C_ParserFree(&parser);
  C_ErrorContextFree(error_context);
  C_LexerFree(lexer);
}

void test_parse_string_literal(void) {
  const char source[1024] =
      "int main() {"
      "   \"Hello, World\";"
      "return 0;"
      "}";

  C_ErrorContext *error_context = C_ErrorContextCreate();
  if (!error_context) {
    fprintf(stderr, "Failed to allocate memory for error_context\n");
    return;
  }

  C_Lexer  *lexer  = C_LexerCreate(source);
  C_Token  *tokens = C_LexerLex(lexer);
  C_Parser *parser = C_ParserCreate(tokens, error_context, "test_filename.c");

  C_AstProgram *program = C_ParserParse(parser);

  TEST_ASSERT_NOT_NULL(program);
  TEST_ASSERT_EQUAL(1, arrlen(program->function_declarations));

  C_AstFunctionDeclaration *func = program->function_declarations[0];
  TEST_ASSERT_EQUAL_STRING("main", StringGetCstr(&func->function_name));
  TEST_ASSERT_NOT_NULL(func->body);
  TEST_ASSERT_EQUAL(2, arrlen(func->body->statements));

  C_AstStatement *stmt0 = func->body->statements[0];
  TEST_ASSERT_NOT_NULL(stmt0);
  TEST_ASSERT_EQUAL(C_STATEMENT_EXPRESSION, stmt0->type);
  TEST_ASSERT_NOT_NULL(stmt0->expression);
  TEST_ASSERT_EQUAL(C_CONSTANT, stmt0->expression->type);
  TEST_ASSERT_EQUAL(C_AST_CONSTANT_STRING,
                    stmt0->expression->constant->type);
  TEST_ASSERT_EQUAL_STRING(
      "Hello, World",
      StringGetCstr(&stmt0->expression->constant->value.string_value));

  C_AstStatement *stmt1 = func->body->statements[1];
  TEST_ASSERT_NOT_NULL(stmt1);
  TEST_ASSERT_EQUAL(C_STATEMENT_RETURN, stmt1->type);
  TEST_ASSERT_NOT_NULL(stmt1->return_statement);
  TEST_ASSERT_EQUAL(0,
                    stmt1->return_statement->value->constant->value.int_value);

  C_ParserFreeProgram(&program);
  C_ParserFree(&parser);
  C_ErrorContextFree(error_context);
  C_LexerFree(lexer);
}

void test_parse_char_literal(void) {
  const char source[1024] =
      "int main() {"
      "   'a';"
      "return 0;"
      "}";

  C_ErrorContext *error_context = C_ErrorContextCreate();
  if (!error_context) {
    fprintf(stderr, "Failed to allocate memory for error_context\n");
    return;
  }

  C_Lexer  *lexer  = C_LexerCreate(source);
  C_Token  *tokens = C_LexerLex(lexer);
  C_Parser *parser = C_ParserCreate(tokens, error_context, "test_filename.c");

  C_AstProgram *program = C_ParserParse(parser);

  TEST_ASSERT_NOT_NULL(program);
  TEST_ASSERT_EQUAL(1, arrlen(program->function_declarations));

  C_AstFunctionDeclaration *func = program->function_declarations[0];
  TEST_ASSERT_EQUAL(2, arrlen(func->body->statements));

  C_AstStatement *stmt0 = func->body->statements[0];
  TEST_ASSERT_NOT_NULL(stmt0);
  TEST_ASSERT_EQUAL(C_STATEMENT_EXPRESSION, stmt0->type);
  TEST_ASSERT_NOT_NULL(stmt0->expression);
  TEST_ASSERT_EQUAL(C_CONSTANT, stmt0->expression->type);
  TEST_ASSERT_EQUAL(C_AST_CONSTANT_CHAR, stmt0->expression->constant->type);
  TEST_ASSERT_EQUAL('a', stmt0->expression->constant->value.int_value);

  C_ParserFreeProgram(&program);
  C_ParserFree(&parser);
  C_ErrorContextFree(error_context);
  C_LexerFree(lexer);
}

void test_parse_char_declaration(void) {
  const char source[1024] =
      "int main() {"
      "   char c = 'a';"
      "return 0;"
      "}";

  C_ErrorContext *error_context = C_ErrorContextCreate();
  if (!error_context) {
    fprintf(stderr, "Failed to allocate memory for error_context\n");
    return;
  }

  C_Lexer  *lexer  = C_LexerCreate(source);
  C_Token  *tokens = C_LexerLex(lexer);
  C_Parser *parser = C_ParserCreate(tokens, error_context, "test_filename.c");

  C_AstProgram *program = C_ParserParse(parser);

  TEST_ASSERT_NOT_NULL(program);
  TEST_ASSERT_EQUAL(1, arrlen(program->function_declarations));

  C_AstFunctionDeclaration *func = program->function_declarations[0];
  TEST_ASSERT_EQUAL(2, arrlen(func->body->statements));

  C_AstStatement *stmt0 = func->body->statements[0];
  TEST_ASSERT_NOT_NULL(stmt0);
  TEST_ASSERT_EQUAL(C_STATEMENT_ASSIGNMENT, stmt0->type);
  TEST_ASSERT_NOT_NULL(stmt0->assignment);
  TEST_ASSERT_EQUAL_STRING("c",
                           StringGetCstr(&stmt0->assignment->variable_name));
  TEST_ASSERT_NOT_NULL(stmt0->assignment->expression);
  TEST_ASSERT_EQUAL(C_CONSTANT, stmt0->assignment->expression->type);
  TEST_ASSERT_EQUAL(C_AST_CONSTANT_CHAR,
                    stmt0->assignment->expression->constant->type);
  TEST_ASSERT_EQUAL('a', stmt0->assignment->expression->constant->value.int_value);

  C_ParserFreeProgram(&program);
  C_ParserFree(&parser);
  C_ErrorContextFree(error_context);
  C_LexerFree(lexer);
}

void test_parse_deref_expression(void) {
  const char source[1024] =
      "int main() {"
      "   int x = 42;"
      "   return *x;"
      "}";

  C_ErrorContext *error_context = C_ErrorContextCreate();
  if (!error_context) {
    fprintf(stderr, "Failed to allocate memory for error_context\n");
    return;
  }

  C_Lexer  *lexer  = C_LexerCreate(source);
  C_Token  *tokens = C_LexerLex(lexer);
  C_Parser *parser = C_ParserCreate(tokens, error_context, "test_filename.c");

  C_AstProgram *program = C_ParserParse(parser);

  TEST_ASSERT_NOT_NULL(program);
  TEST_ASSERT_EQUAL(1, arrlen(program->function_declarations));

  C_AstFunctionDeclaration *func = program->function_declarations[0];
  TEST_ASSERT_EQUAL(2, arrlen(func->body->statements));

  C_AstStatement *ret_stmt = func->body->statements[1];
  TEST_ASSERT_EQUAL(C_STATEMENT_RETURN, ret_stmt->type);
  TEST_ASSERT_NOT_NULL(ret_stmt->return_statement);
  TEST_ASSERT_NOT_NULL(ret_stmt->return_statement->value);
  TEST_ASSERT_EQUAL(C_UNARY_EXPRESSION,
                    ret_stmt->return_statement->value->type);
  TEST_ASSERT_EQUAL(C_UNARY_DEREF,
                    ret_stmt->return_statement->value->unary->type);

  C_ParserFreeProgram(&program);
  C_ParserFree(&parser);
  C_ErrorContextFree(error_context);
  C_LexerFree(lexer);
}

void test_parse_address_of_expression(void) {
  const char source[1024] =
      "int main() {"
      "   int x = 42;"
      "   int *p = &x;"
      "return 0;"
      "}";

  C_ErrorContext *error_context = C_ErrorContextCreate();
  if (!error_context) {
    fprintf(stderr, "Failed to allocate memory for error_context\n");
    return;
  }

  C_Lexer  *lexer  = C_LexerCreate(source);
  C_Token  *tokens = C_LexerLex(lexer);
  C_Parser *parser = C_ParserCreate(tokens, error_context, "test_filename.c");

  C_AstProgram *program = C_ParserParse(parser);

  TEST_ASSERT_NOT_NULL(program);
  TEST_ASSERT_EQUAL(1, arrlen(program->function_declarations));

  C_AstFunctionDeclaration *func = program->function_declarations[0];
  TEST_ASSERT_EQUAL(3, arrlen(func->body->statements));

  C_AstStatement *assign_stmt = func->body->statements[1];
  TEST_ASSERT_EQUAL(C_STATEMENT_ASSIGNMENT, assign_stmt->type);
  TEST_ASSERT_EQUAL(1, assign_stmt->assignment->pointer_depth);
  TEST_ASSERT_EQUAL(C_UNARY_EXPRESSION,
                    assign_stmt->assignment->expression->type);
  TEST_ASSERT_EQUAL(C_UNARY_ADDRESS_OF,
                    assign_stmt->assignment->expression->unary->type);

  C_ParserFreeProgram(&program);
  C_ParserFree(&parser);
  C_ErrorContextFree(error_context);
  C_LexerFree(lexer);
}

void test_parse_if_statement(void) {
  const char source[1024] =
      "int main() {"
      "  if (1) {"
      "    return 0;"
      "  }"
      "  return 1;"
      "}";

  C_ErrorContext *error_context = C_ErrorContextCreate();
  if (!error_context) {
    fprintf(stderr, "Failed to allocate memory for error_context\n");
    return;
  }

  C_Lexer  *lexer  = C_LexerCreate(source);
  C_Token  *tokens = C_LexerLex(lexer);
  C_Parser *parser = C_ParserCreate(tokens, error_context, "test_filename.c");

  C_AstProgram *program = C_ParserParse(parser);

  TEST_ASSERT_NOT_NULL(program);
  TEST_ASSERT_EQUAL(1, arrlen(program->function_declarations));

  C_AstFunctionDeclaration *func = program->function_declarations[0];
  TEST_ASSERT_EQUAL(2, arrlen(func->body->statements));

  C_AstStatement *if_stmt = func->body->statements[0];
  TEST_ASSERT_EQUAL(C_STATEMENT_IF, if_stmt->type);
  TEST_ASSERT_NOT_NULL(if_stmt->if_statement);
  TEST_ASSERT_NOT_NULL(if_stmt->if_statement->condition);
  TEST_ASSERT_EQUAL(C_CONSTANT, if_stmt->if_statement->condition->type);
  TEST_ASSERT_EQUAL(
      C_AST_CONSTANT_INT,
      if_stmt->if_statement->condition->constant->type);
  TEST_ASSERT_EQUAL(
      1,
      if_stmt->if_statement->condition->constant->value.int_value);
  TEST_ASSERT_NOT_NULL(if_stmt->if_statement->block);
  TEST_ASSERT_EQUAL(C_STATEMENT_BLOCK, if_stmt->if_statement->block->type);
  TEST_ASSERT_EQUAL(1, arrlen(if_stmt->if_statement->block->block->statements));

  C_AstStatement *body_stmt =
      if_stmt->if_statement->block->block->statements[0];
  TEST_ASSERT_EQUAL(C_STATEMENT_RETURN, body_stmt->type);
  TEST_ASSERT_NOT_NULL(body_stmt->return_statement);
  TEST_ASSERT_EQUAL(
      0,
      body_stmt->return_statement->value->constant->value.int_value);

  C_AstStatement *ret_stmt = func->body->statements[1];
  TEST_ASSERT_EQUAL(C_STATEMENT_RETURN, ret_stmt->type);
  TEST_ASSERT_EQUAL(
      1,
      ret_stmt->return_statement->value->constant->value.int_value);

  C_ParserFreeProgram(&program);
  C_ParserFree(&parser);
  C_ErrorContextFree(error_context);
  C_LexerFree(lexer);
}

void test_parse_if_else_statement(void) {
  const char source[1024] =
      "int main() {"
      "  if (1) {"
      "    return 0;"
      "  } else {"
      "    return 1;"
      "  }"
      "}";

  C_ErrorContext *error_context = C_ErrorContextCreate();
  if (!error_context) {
    fprintf(stderr, "Failed to allocate memory for error_context\n");
    return;
  }

  C_Lexer  *lexer  = C_LexerCreate(source);
  C_Token  *tokens = C_LexerLex(lexer);
  C_Parser *parser = C_ParserCreate(tokens, error_context, "test_filename.c");

  C_AstProgram *program = C_ParserParse(parser);

  TEST_ASSERT_NOT_NULL(program);
  TEST_ASSERT_EQUAL(1, arrlen(program->function_declarations));

  C_AstFunctionDeclaration *func = program->function_declarations[0];
  TEST_ASSERT_EQUAL(1, arrlen(func->body->statements));

  C_AstStatement *if_stmt = func->body->statements[0];
  TEST_ASSERT_EQUAL(C_STATEMENT_IF, if_stmt->type);
  TEST_ASSERT_NOT_NULL(if_stmt->if_statement);
  TEST_ASSERT(if_stmt->if_statement->has_else);
  TEST_ASSERT_NOT_NULL(if_stmt->if_statement->else_block);

  TEST_ASSERT_EQUAL(
      1,
      if_stmt->if_statement->condition->constant->value.int_value);

  TEST_ASSERT_EQUAL(C_STATEMENT_BLOCK, if_stmt->if_statement->block->type);
  TEST_ASSERT_EQUAL(
      0,
      if_stmt->if_statement->block->block->statements[0]
          ->return_statement->value->constant->value.int_value);

  TEST_ASSERT_EQUAL(C_STATEMENT_BLOCK, if_stmt->if_statement->else_block->type);
  TEST_ASSERT_EQUAL(
      1,
      if_stmt->if_statement->else_block->block->statements[0]
          ->return_statement->value->constant->value.int_value);

  C_ParserFreeProgram(&program);
  C_ParserFree(&parser);
  C_ErrorContextFree(error_context);
  C_LexerFree(lexer);
}

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  UNITY_BEGIN();
  RUN_TEST(test_parse_function_declaration);
  RUN_TEST(test_parse_string_literal);
  RUN_TEST(test_parse_char_literal);
  RUN_TEST(test_parse_char_declaration);
  RUN_TEST(test_parse_deref_expression);
  RUN_TEST(test_parse_address_of_expression);
  RUN_TEST(test_parse_if_statement);
  RUN_TEST(test_parse_if_else_statement);
  return UNITY_END();
}
