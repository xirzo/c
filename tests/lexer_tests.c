#include "unity.h"
#include "lexer.h"

void setUp(void) {}

void tearDown(void) {}

void test_lex_numbers(void) {
    const char source[1024] = "1 12 123";
    c_lexer *lexer = c_lexer_create(source);
    c_token *tokens = c_lexer_lex(lexer);

    TEST_ASSERT_EQUAL_STRING("1", tokens[0].string);
    TEST_ASSERT_EQUAL(C_INTEGER_LITERAL, tokens[0].type);
    TEST_ASSERT_EQUAL_STRING("12", tokens[1].string);
    TEST_ASSERT_EQUAL(C_INTEGER_LITERAL, tokens[1].type);
    TEST_ASSERT_EQUAL_STRING("123", tokens[2].string);
    TEST_ASSERT_EQUAL(C_INTEGER_LITERAL, tokens[2].type);

    c_lexer_free(lexer);
    c_lexer_free_tokens(tokens);
}

void test_default_main(void) {
    const char source[1024] =
        "int main() {"
        "   return 0;"
        "}";
    c_lexer *lexer = c_lexer_create(source);
    c_token *tokens = c_lexer_lex(lexer);

    TEST_ASSERT_EQUAL(C_INTEGER, tokens[0].type);
    TEST_ASSERT_EQUAL(C_IDENTIFIER, tokens[1].type);
    TEST_ASSERT_EQUAL_STRING("main", tokens[1].string);
    TEST_ASSERT_EQUAL(C_LPAREN, tokens[2].type);
    TEST_ASSERT_EQUAL(C_RPAREN, tokens[3].type);
    TEST_ASSERT_EQUAL(C_LBRACE, tokens[4].type);
    TEST_ASSERT_EQUAL(C_RETURN, tokens[5].type);
    TEST_ASSERT_EQUAL(C_INTEGER_LITERAL, tokens[6].type);
    TEST_ASSERT_EQUAL_STRING("0", tokens[6].string);
    TEST_ASSERT_EQUAL(C_SEMICOLON, tokens[7].type);
    TEST_ASSERT_EQUAL(C_RBRACE, tokens[8].type);

    c_lexer_free(lexer);
    c_lexer_free_tokens(tokens);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_lex_numbers);
    RUN_TEST(test_default_main);
    return UNITY_END();
}
