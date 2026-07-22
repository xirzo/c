#include <stdio.h>
#include <stdlib.h>
#include "code_generator.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "stb_ds.h"
#include "str.h"
#include "utils.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <source_file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  char *source = C_ReadFileToBuffer(argv[1]);

  if (!source) {
    return EXIT_FAILURE;
  }

  C_ErrorContext *error_context = C_ErrorContextCreate();

  if (!error_context) {
    fprintf(stderr, "Failed to allocate memory for error_context\n");
    free(source);
    return EXIT_FAILURE;
  }

  C_Lexer *lexer  = C_LexerCreate(source);
  C_Token *tokens = C_LexerLex(lexer);

  C_Parser *parser = C_ParserCreate(tokens, error_context, argv[1]);

  C_AstProgram *program = C_ParserParse(parser);

  if (C_ErrorContextHasErrors(error_context)) {
    C_ErrorContextPrint(error_context, stderr);
    C_LexerFree(lexer);
    C_ParserFreeProgram(program);
    C_ErrorContextFree(error_context);
    C_ParserFree(parser);
    free(source);
    return EXIT_FAILURE;
  }

  FILE *file = fopen("c.asm", "w");

  if (!file) {
    C_ParserFreeProgram(program);
    C_ParserFree(parser);
    EXIT_WITH_ERROR("Failed to open file for writing\n");
  }

  char **asm_lines = C_CodeGenEmit(program);

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
  C_LexerFree(lexer);
  C_ParserFreeProgram(program);
  C_ErrorContextFree(error_context);
  C_ParserFree(parser);
  free(source);
  return 0;
}
