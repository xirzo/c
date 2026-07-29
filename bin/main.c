#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "code_generator.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "preprocessor.h"
#include "stb_ds.h"
#include "str.h"
#include "utils.h"
#include "xi_string.h"

static String *object_filenames = NULL;

static const char *include_paths[] = {
    ".",
    "/usr/include",
    "/usr/local/include",
    "./include",
    NULL
};

static int compile_file(const char *filepath, bool emit_entry) {
    char *source = C_ReadFileToBuffer(filepath);
    if (!source) {
        fprintf(stderr, "Failed to read file: %s\n", filepath);
        return 1;
    }

    String str_source = StringCreate(source);
    free(source);

    String processed_source = PreprocessorProcessWithIncludes(&str_source, include_paths);
    if (!processed_source.data) {
        StringFree(&str_source);
        return 1;
    }

    if (getenv("DEBUG_PREPROCESSOR")) {
        printf("========== PREPROCESSED SOURCE (%s) ==========\n", filepath);
        printf("%s\n", StringGetCstr(&processed_source));
        printf("===============================================\n");
    }

    C_ErrorContext *error_context = C_ErrorContextCreate();
    if (!error_context) {
        StringFree(&str_source);
        StringFree(&processed_source);
        return 1;
    }

    C_Lexer *lexer = C_LexerCreate(StringGetCstr(&processed_source));
    if (!lexer) {
        C_ErrorContextFree(error_context);
        StringFree(&str_source);
        StringFree(&processed_source);
        return 1;
    }

    C_Token *tokens = C_LexerLex(lexer);
    if (!tokens) {
        C_LexerFree(lexer);
        C_ErrorContextFree(error_context);
        StringFree(&str_source);
        StringFree(&processed_source);
        return 1;
    }

    C_Parser *parser = C_ParserCreate(tokens, error_context, filepath);
    if (!parser) {
        C_LexerFree(lexer);
        C_ErrorContextFree(error_context);
        StringFree(&str_source);
        StringFree(&processed_source);
        return 1;
    }

    C_AstProgram *program = C_ParserParse(parser);

    if (C_ErrorContextHasErrors(error_context)) {
        C_ErrorContextPrint(error_context, stderr);
        C_LexerFree(lexer);
        C_ParserFreeProgram(&program);
        C_ErrorContextFree(error_context);
        C_ParserFree(&parser);
        StringFree(&str_source);
        StringFree(&processed_source);
        return 1;
    }

    const char *slash = strrchr(filepath, '/');
    const char *name = slash ? slash + 1 : filepath;
    const char *dot = strrchr(name, '.');
    size_t name_len = dot ? (size_t)(dot - name) : strlen(name);
    String base_name = StringCreateEmpty(name_len);
    StringAppendCstr(&base_name, name);
    base_name.length = name_len;
    base_name.data[name_len] = '\0';

    String asm_filename = StringCreateEmpty(0);
    StringPrintf(&asm_filename, "%s.asm", StringGetCstr(&base_name));

    FILE *file = fopen(StringGetCstr(&asm_filename), "w");
    if (!file) {
        C_LexerFree(lexer);
        C_ParserFreeProgram(&program);
        C_ParserFree(&parser);
        C_ErrorContextFree(error_context);
        StringFree(&base_name);
        StringFree(&asm_filename);
        StringFree(&str_source);
        StringFree(&processed_source);
        return 1;
    }

    String *asm_lines = C_CodeGenEmit(program, emit_entry);

    for (int i = 0; i < arrlen(asm_lines); i++) {
        fprintf(file, "%s\n", StringGetCstr(&asm_lines[i]));
    }
    fclose(file);

    String object_filename = StringCreateEmpty(0);
    StringPrintf(&object_filename, "%s.o", StringGetCstr(&base_name));

    String asm_command = StringCreateEmpty(30);
    StringPrintf(&asm_command, "nasm -f elf64 %s -o %s",
                 StringGetCstr(&asm_filename), StringGetCstr(&object_filename));

    LOG_DEBUG("Running: %s\n", StringGetCstr(&asm_command));
    system(StringGetCstr(&asm_command));

    for (int i = 0; i < arrlen(asm_lines); i++) {
        StringFree(&asm_lines[i]);
    }
    arrfree(asm_lines);

    C_LexerFree(lexer);
    C_ParserFreeProgram(&program);
    C_ParserFree(&parser);
    C_ErrorContextFree(error_context);

    StringFree(&base_name);
    StringFree(&asm_filename);
    StringFree(&asm_command);
    StringFree(&str_source);
    StringFree(&processed_source);

    arrput(object_filenames, StringDuplicate(&object_filename));
    StringFree(&object_filename);

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source_file>...\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *output_name = NULL;
    size_t      output_name_len = 0;
    int         first = 1;
    int         status = 0;

    for (int i = 1; i < argc; i++) {
        const char *ext = strrchr(argv[i], '.');
        if (!ext || strcmp(ext, ".c") != 0) {
            fprintf(stderr, "Skipping non-.c file: %s\n", argv[i]);
            continue;
        }

        if (first) {
            const char *slash = strrchr(argv[i], '/');
            const char *name = slash ? slash + 1 : argv[i];
            output_name_len = ext - name;
            output_name = name;
            first = 0;
        }

        if (compile_file(argv[i], i == 1) != 0) {
            status = 1;
        }
    }

    if (status != 0) {
        return EXIT_FAILURE;
    }

    String output_filename = StringCreateEmpty(output_name_len);
    StringAppendCstr(&output_filename, output_name);
    output_filename.length = output_name_len;
    output_filename.data[output_name_len] = '\0';

    String ld_command = StringCreateEmpty(0);
    StringAppendCstr(&ld_command, "ld");
    for (int i = 0; i < arrlen(object_filenames); i++) {
        StringAppendCstr(&ld_command, " ");
        StringAppend(&ld_command, &object_filenames[i]);
    }
    StringAppendCstr(&ld_command, " -o ");
    StringAppend(&ld_command, &output_filename);

    LOG_DEBUG("Running: %s\n", StringGetCstr(&ld_command));
    system(StringGetCstr(&ld_command));

    for (int i = 0; i < arrlen(object_filenames); i++) {
        StringFree(&object_filenames[i]);
    }
    arrfree(object_filenames);
    StringFree(&ld_command);
    StringFree(&output_filename);

    LOG_DEBUG("Compilation complete!\n");
    return 0;
}
