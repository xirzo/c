#include "lexer.h"
#include "stb_ds.h"

int main(void) {
    const char source[1024] =
        "int main() {"
        "   return 0;"
        "}";

    c_lexer *lexer = c_lexer_create(source);

    c_token *tokens = c_lexer_lex(lexer);

    for (int i = 0; i < arrlen(tokens); i++) {}

    c_lexer_free(lexer);
    c_lexer_free_tokens(tokens);
    return 0;
}
