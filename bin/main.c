#include <stdio.h>
#include <stdlib.h>
#include "code_generator.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "stb_ds.h"
#include "str.h"
#include "utils.h"
#include "xi_string.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <source_file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  String filepath = StringCreate(argv[1]);

  if (!StringEndsWith(&filepath, ".c")) {
    fprintf(stderr, "Provided file \"%s\" is not a c file (add .c format)\n",
            StringGetCstr(&filepath));
    return EXIT_FAILURE;
  }

  size_t  chunks_count    = 0;
  String *filename_chunks = StringSplit(&filepath, ".", &chunks_count);
  if (!filename_chunks || chunks_count != 2) {
    StringFree(&filepath);
    return EXIT_FAILURE;
  }

  String filename_no_format = StringDuplicate(&filename_chunks[0]);

  for (size_t i = 0; i < chunks_count; i++) {
    StringFree(&filename_chunks[i]);
  }
  free(filename_chunks);

  LOG_DEBUG("Filename: %s\n", StringGetCstr(&filename_no_format));

  char *source = C_ReadFileToBuffer(StringGetCstr(&filepath));
  if (!source) {
    StringFree(&filepath);
    StringFree(&filename_no_format);
    return EXIT_FAILURE;
  }

  C_ErrorContext *error_context = C_ErrorContextCreate();
  if (!error_context) {
    fprintf(stderr, "Failed to allocate memory for error_context\n");
    free(source);
    StringFree(&filepath);
    StringFree(&filename_no_format);
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
    StringFree(&filepath);
    StringFree(&filename_no_format);
    return EXIT_FAILURE;
  }

  String asm_filename = StringDuplicate(&filename_no_format);
  StringAppendCstr(&asm_filename, ".asm");

  FILE *file = fopen(StringGetCstr(&asm_filename), "w");

  if (!file) {
    C_ParserFreeProgram(program);
    C_ParserFree(parser);
    StringFree(&asm_filename);
    EXIT_WITH_ERROR("Failed to open file for writing");
  }

  char **asm_lines = C_CodeGenEmit(program);

  for (int i = 0; i < arrlen(asm_lines); i++) {
    fprintf(file, "%s\n", asm_lines[i]);
  }
  fclose(file);

  String object_filename = StringCreateEmpty(0);
  StringPrintf(&object_filename, "%s.o", StringGetCstr(&filename_no_format));

  String asm_command = StringCreateEmpty(30);
  StringPrintf(&asm_command, "nasm -f elf64 %s -o %s",
               StringGetCstr(&asm_filename), StringGetCstr(&object_filename));

  String ld_command = StringCreateEmpty(0);
  StringPrintf(&ld_command, "ld %s -o %s", StringGetCstr(&object_filename),
               StringGetCstr(&filename_no_format));

  system(StringGetCstr(&asm_command));
  system(StringGetCstr(&ld_command));

  for (int i = 0; i < arrlen(asm_lines); i++) {
    free(asm_lines[i]);
  }
  arrfree(asm_lines);
  C_LexerFree(lexer);
  C_ParserFreeProgram(program);
  C_ErrorContextFree(error_context);
  C_ParserFree(parser);
  free(source);
  StringFree(&filepath);
  StringFree(&filename_no_format);
  StringFree(&asm_filename);
  StringFree(&object_filename);
  StringFree(&asm_command);
  StringFree(&ld_command);
  return 0;
}
