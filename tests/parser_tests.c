#include "unity.h"
#include "lexer.h"
#include "stb_ds.h"
#include "parser.h"

void setUp(void) {}

void tearDown(void) {}

void test_parse_function_declaration(void) {
    const char source[1024] =
        "int main() {"
        "   return 0;"
        "}";

    c_error_context *error_context = c_error_context_create();

    if (!error_context) {
        fprintf(stderr, "Failed to allocate memory for error_context\n");
        return;
    }

    c_lexer *lexer = c_lexer_create(source);
    c_token *tokens = c_lexer_lex(lexer);
    c_parser *parser =
        c_parser_create(tokens, error_context, "test_filename.c");

    c_ast_function_declaration *func =
        c_parser_parse_function_declaration(parser);

    TEST_ASSERT_NOT_NULL(func);
    TEST_ASSERT_EQUAL_STRING("main", func->function_name);
    TEST_ASSERT_NOT_NULL(func->body);
    TEST_ASSERT_EQUAL(1, arrlen(func->body->statements));

    c_ast_statement *stmt = func->body->statements[0];
    TEST_ASSERT_EQUAL(C_STATEMENT_RETURN, stmt->type);
    TEST_ASSERT_NOT_NULL(stmt->return_statement);
    TEST_ASSERT_EQUAL(0, stmt->return_statement->value->constant->value);

    c_ast_free_function_declaration(func);
    c_parser_free(parser);
    c_lexer_free(lexer);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_parse_function_declaration);
    return UNITY_END();
}
