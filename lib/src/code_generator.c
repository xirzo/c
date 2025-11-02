#include "code_generator.h"
#include "parser.h"
#include "stb_ds.h"
#include "utils.h"
#include "str.h"

// section .text
// global _start
//
// _start:
//     call main    ; Call our main function
//
//     ; Exit with main's return value
//     mov edi, eax ; Move return value to exit status
//     mov eax, 60  ; sys_exit
//     syscall
//
// main:
//     ; Function prologue
//     push rbp
//     mov rbp, rsp
//
//     ; Function body - return 22
//     mov eax, 22
//
//     ; Function epilogue
//     pop rbp
//     ret

#define MAX_LITERAL_LENGTH 64

#define ADD_TO_LINES(new_lines)                   \
    for (int i = 0; i < arrlen(new_lines); i++) { \
        arrput(lines, new_lines[i]);              \
    }

char **c_code_gen_emit(c_ast_program *program) {
    char **lines = NULL;

    arrput(lines, strdup("global _start"));
    arrput(lines, strdup(""));
    arrput(lines, strdup("section .text"));
    arrput(lines, strdup("_start:"));
    arrput(lines, strdup("    call main"));
    arrput(lines, strdup(""));
    arrput(lines, strdup("    mov edi, eax"));
    arrput(lines, strdup("    mov eax, 60"));
    arrput(lines, strdup("    syscall"));
    arrput(lines, strdup(""));

    for (int i = 0; i < arrlen(program->function_declarations); i++) {
        char **function_lines = c_code_gen_emit_function_declaration(
            program->function_declarations[i]);

        for (int i = 0; i < arrlen(function_lines); i++) {
            arrput(lines, function_lines[i]);
        }

        arrfree(function_lines);
    }

    return lines;
}

char **c_code_gen_emit_constant(c_ast_constant *constant) {
    char **lines = NULL;

    char *line = malloc(MAX_LITERAL_LENGTH);
    snprintf(line, MAX_LITERAL_LENGTH, "    mov eax, %d", constant->value);
    arrput(lines, line);

    return lines;
}

char **c_code_gen_emit_function_call(c_ast_function_call *function_call) {
    char **lines = NULL;

    char *function_label = malloc(strlen(function_call->function_name) + 10);
    snprintf(function_label,
             strlen(function_call->function_name) + 10,
             "    call %s",
             function_call->function_name);
    arrput(lines, function_label);

    return lines;
}

char **c_code_gen_emit_expression(c_ast_expression *expression) {
    char **lines = NULL;

    switch (expression->type) {
        case C_CONSTANT: {
            char **constant_lines =
                c_code_gen_emit_constant(expression->constant);
            ADD_TO_LINES(constant_lines);
            arrfree(constant_lines);
            break;
        }
        case C_FUNCTION_CALL: {
            char **function_call_lines =
                c_code_gen_emit_function_call(expression->function_call);
            ADD_TO_LINES(function_call_lines);
            arrfree(function_call_lines);
            break;
        }
        default:
            arrfree(lines);
            EXIT_WITH_ERROR("Got unsupported type for expression emit");
    }

    return lines;
}

char **c_code_gen_emit_return(c_ast_return *ret) {
    char **lines = NULL;

    if (ret->value) {
        char **expression_lines = c_code_gen_emit_expression(ret->value);
        ADD_TO_LINES(expression_lines);
        arrfree(expression_lines);
    }

    return lines;
}

char **c_code_gen_emit_statement(c_ast_statement *statement) {
    char **lines = NULL;

    switch (statement->type) {
        case C_STATEMENT_BLOCK: {
            char **block_lines = c_code_gen_emit_block(statement->block);
            ADD_TO_LINES(block_lines);
            arrfree(block_lines);
            break;
        }
        case C_STATEMENT_RETURN: {
            char **return_lines =
                c_code_gen_emit_return(statement->return_statement);
            ADD_TO_LINES(return_lines);
            arrfree(return_lines);
            break;
        }
        case C_STATEMENT_FUNCTION_DECLARATION: {
            char **function_declaration_lines =
                c_code_gen_emit_function_declaration(
                    statement->function_declaration);
            ADD_TO_LINES(function_declaration_lines);
            arrfree(function_declaration_lines);
            break;
        }
        default:
            arrfree(lines);
            EXIT_WITH_ERROR("Got unsupported type for statement emit");
    }

    return lines;
}

char **c_code_gen_emit_block(c_ast_block *block) {
    char **lines = NULL;

    for (int i = 0; i < arrlen(block->statements); i++) {
        char **statement_lines =
            c_code_gen_emit_statement(block->statements[i]);

        ADD_TO_LINES(statement_lines);
        arrfree(statement_lines);
    }

    return lines;
}

char **c_code_gen_emit_function_declaration(
    c_ast_function_declaration *function_declaration) {
    char **lines = NULL;

    char *function_label =
        malloc(strlen(function_declaration->function_name) + 2);
    snprintf(function_label,
             strlen(function_declaration->function_name) + 2,
             "%s:",
             function_declaration->function_name);
    arrput(lines, function_label);

    arrput(lines, strdup("    push rbp"));
    arrput(lines, strdup("    mov rbp, rsp"));
    char **body_lines = c_code_gen_emit_block(function_declaration->body);
    ADD_TO_LINES(body_lines);
    arrfree(body_lines);

    arrput(lines, strdup(""));
    arrput(lines, strdup("    pop rbp"));
    arrput(lines, strdup("    ret"));
    arrput(lines, strdup(""));

    return lines;
}
