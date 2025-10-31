#include "lexer.h"
#include "parser.h"
#include "stb_ds.h"

int main(void) {
    const char source[1024] =
        "int main() {"
        "   return 0;"
        "}";

    c_lexer *lexer = c_lexer_create(source);
    c_token *tokens = c_lexer_lex(lexer);
    c_lexer_free(lexer);

    c_parser *parser = c_parser_create(tokens);

    c_ast_program *program = c_parser_parse(parser);

    c_parser_free(parser);
    c_parser_free_program(program);
    return 0;
}
