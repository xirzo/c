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
    C_ErrorContext *error_context = C_ErrorContextCreate();
    if (!error_context) {
        fprintf(stderr, "Failed to allocate memory for error_context\n");
        return;
    }
    C_Lexer *lexer = C_LexerCreate(source);
    C_Token *tokens = C_LexerLex(lexer);
    C_LexerFree(lexer);
    C_Parser *parser =
        C_ParserCreate(tokens, error_context, "test_filename.c");
    C_AstProgram *program = C_ParserParse(parser);

    char **asm_lines = C_CodeGenEmit(program);

    for (int i = 0; i < arrlen(asm_lines); i++) {
        strcat(result, asm_lines[i]);
        if (i < arrlen(asm_lines) - 1) {
            strcat(result, "\n");
        }
    }

    C_ParserFreeProgram(program);
    C_ParserFree(parser);

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
