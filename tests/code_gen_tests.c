#include <string.h>
#include "code_generator.h"
#include "unity.h"
#include "lexer.h"
#include "stb_ds.h"
#include "parser.h"

void setUp(void) {}

void tearDown(void) {}

void test_code_gen_main_function(void) {
    const char source[1024] =
        "int main() {"
        "   return 69;"
        "}";
    char result[4096] = {0};
    c_lexer *lexer = c_lexer_create(source);
    c_token *tokens = c_lexer_lex(lexer);
    c_lexer_free(lexer);
    c_parser *parser = c_parser_create(tokens);
    c_ast_program *program = c_parser_parse(parser);

    char **asm_lines = c_code_gen_emit(program);

    for (int i = 0; i < arrlen(asm_lines); i++) {
        strcat(result, asm_lines[i]);
        if (i < arrlen(asm_lines) - 1) {
            strcat(result, "\n");
        }
    }

    c_parser_free_program(program);
    c_parser_free(parser);

    for (int i = 0; i < arrlen(asm_lines); i++) {
        free(asm_lines[i]);
    }
    arrfree(asm_lines);

    const char expected[] =
        "global _start\n"
        "\n"
        "section .text\n"
        "_start:\n"
        "    call main\n"
        "\n"
        "    mov edi, eax\n"
        "    mov eax, 60\n"
        "    syscall\n"
        "\n"
        "main:\n"
        "    push rbp\n"
        "    mov rbp, rsp\n"
        "    mov eax, 69\n"
        "\n"
        "    pop rbp\n"
        "    ret\n";

    TEST_ASSERT_EQUAL_STRING(expected, result);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_code_gen_main_function);
    return UNITY_END();
}
