#include <stdio.h>
#include "code_generator.h"
#include "lexer.h"
#include "parser.h"
#include "utils.h"
#include "stb_ds.h"

int main(void) {
    const char source[1024] =
        "int test() {"
        "   return 10;"
        "}"
        ""
        "int main() {"
        "   test();"
        "   return test();"
        "}";

    c_lexer *lexer = c_lexer_create(source);
    c_token *tokens = c_lexer_lex(lexer);
    c_lexer_free(lexer);

    c_parser *parser = c_parser_create(tokens);

    c_ast_program *program = c_parser_parse(parser);

    FILE *file = fopen("c.asm", "w");

    if (!file) {
        c_parser_free_program(program);
        c_parser_free(parser);
        EXIT_WITH_ERROR("Failed to open file for writing\n");
    }

    char **asm_lines = c_code_gen_emit(program);

    for (int i = 0; i < arrlen(asm_lines); i++) {
        fprintf(file, "%s\n", asm_lines[i]);
    }

    fclose(file);

    for (int i = 0; i < arrlen(asm_lines); i++) {
        free(asm_lines[i]);
    }
    arrfree(asm_lines);

    c_parser_free_program(program);

    c_parser_free(parser);

    return 0;
}
