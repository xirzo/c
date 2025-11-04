#include <stdio.h>
#include <stdlib.h>
#include "code_generator.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "utils.h"
#include "stb_ds.h"

int main(void) {
    // TODO: fix naming conflicts
    // (maybe should not name variabls
    // rather just identify them by
    // memory pos)
    const char source[1024] =
        "int main() {"
        "   int lol = 8 + ;"
        "   return lol;"
        "}";

    c_error_context *error_context = c_error_context_create();

    if (!error_context) {
        fprintf(stderr, "Failed to allocate memory for error_context\n");
        return EXIT_FAILURE;
    }

    c_lexer *lexer = c_lexer_create(source);
    c_token *tokens = c_lexer_lex(lexer);

    c_parser *parser =
        c_parser_create(tokens, error_context, "test_filename.c");

    c_ast_program *program = c_parser_parse(parser);

    if (c_error_context_has_errors(error_context)) {
        c_error_context_print(error_context, stderr);
        c_lexer_free(lexer);
        c_parser_free_program(program);
        c_error_context_free(error_context);
        c_parser_free(parser);
        return EXIT_FAILURE;
    }

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

    system("nasm -f elf64 c.asm -o c.o");
    system("ld c.o -o c");

    for (int i = 0; i < arrlen(asm_lines); i++) {
        free(asm_lines[i]);
    }
    arrfree(asm_lines);
    c_lexer_free(lexer);
    c_parser_free_program(program);
    c_error_context_free(error_context);
    c_parser_free(parser);
    return 0;
}
