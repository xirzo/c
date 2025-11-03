#include "code_generator.h"
#include "parser.h"
#include "stb_ds.h"
#include "utils.h"
#include "str.h"

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
    arrput(lines, strdup("    mov rdi, rax"));
    arrput(lines, strdup("    mov rax, 60"));
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
    snprintf(line, MAX_LITERAL_LENGTH, "    mov rax, %d", constant->value);
    arrput(lines, line);

    return lines;
}

char **c_code_gen_emit_function_call(c_ast_function_call *function_call,
                                     const char *assign_to_variable) {
    char **lines = NULL;

    char *function_label = malloc(strlen(function_call->function_name) + 10);
    snprintf(function_label,
             strlen(function_call->function_name) + 10,
             "    call %s",
             function_call->function_name);
    arrput(lines, function_label);

    if (assign_to_variable) {
        char *assignment_line = malloc(strlen(assign_to_variable) + 20);
        snprintf(assignment_line,
                 strlen(assign_to_variable) + 20,
                 "    mov qword %s, rax",
                 assign_to_variable);
        arrput(lines, assignment_line);
    }

    return lines;
}

// global _start
//
// section .text
// _start:
//     call main
//     mov edi, eax
//     mov eax, 60
//     syscall
//
// main:
//     push rbp
//     mov rbp, rsp
//
//     sub rsp, 4
//     %define myVar [rbp-4]
//
//     sub rsp, 4
//     %define anotherVar [rbp-8]
//
//     mov dword myVar, 42
//     mov dword anotherVar, 100
//
//     mov eax, myVar
//     add eax, anotherVar
//
//     mov rsp, rbp
//     pop rbp
//     ret

char **c_code_gen_emit_variable_assignment(
    c_ast_variable_assignment *assignment,
    int *current_offset) {
    char **lines = NULL;

    arrput(lines, strdup(""));
    // TODO: get size of the variable type
    arrput(lines, strdup("    sub rsp, 8"));

    *current_offset += 8;

    int offset_digits = snprintf(NULL, 0, "%d", *current_offset);
    size_t length = strlen(assignment->variable_name) + 20 + offset_digits;
    char *variable_label = malloc(length);

    snprintf(variable_label,
             length,
             "    %%define %s [rbp-%d]",
             assignment->variable_name,
             *current_offset);
    arrput(lines, variable_label);
    arrput(lines, strdup(""));

    return lines;
}

char **c_code_gen_emit_expression(c_ast_expression *expression,
                                  int *current_offset) {
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
                c_code_gen_emit_function_call(expression->function_call, NULL);
            ADD_TO_LINES(function_call_lines);
            arrfree(function_call_lines);
            break;
        }
        default:
            arrfree(lines);
            EXIT_WITH_ERROR("Got unsupported type for expression emit: %d\n",
                            expression->type);
    }

    return lines;
}

char **c_code_gen_emit_return(c_ast_return *ret, int *current_offset) {
    char **lines = NULL;

    if (ret->value) {
        char **expression_lines =
            c_code_gen_emit_expression(ret->value, current_offset);
        ADD_TO_LINES(expression_lines);
        arrfree(expression_lines);
    }

    return lines;
}

char **c_code_gen_emit_statement(c_ast_statement *statement,
                                 int *current_offset) {
    char **lines = NULL;

    switch (statement->type) {
        case C_STATEMENT_BLOCK: {
            char **block_lines =
                c_code_gen_emit_block(statement->block, current_offset);
            ADD_TO_LINES(block_lines);
            arrfree(block_lines);
            break;
        }
        case C_STATEMENT_RETURN: {
            char **return_lines = c_code_gen_emit_return(
                statement->return_statement, current_offset);
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
        case C_STATEMENT_EXPRESSION: {
            char **expression_lines = c_code_gen_emit_expression(
                statement->expression, current_offset);
            ADD_TO_LINES(expression_lines);
            arrfree(expression_lines);
            break;
        }
        case C_STATEMENT_ASSIGNMENT: {
            char **assignment_lines = c_code_gen_emit_variable_assignment(
                statement->assignment, current_offset);
            ADD_TO_LINES(assignment_lines);
            arrfree(assignment_lines);

            if (statement->assignment->expression) {
                switch (statement->assignment->expression->type) {
                    case C_FUNCTION_CALL: {
                        char **function_call_lines =
                            c_code_gen_emit_function_call(
                                statement->assignment->expression
                                    ->function_call,
                                statement->assignment->variable_name);
                        ADD_TO_LINES(function_call_lines);
                        arrfree(function_call_lines);
                        break;
                    }
                    case C_CONSTANT: {
                        char **expression_lines = c_code_gen_emit_expression(
                            statement->assignment->expression, current_offset);
                        ADD_TO_LINES(expression_lines);
                        arrfree(expression_lines);

                        char *assignment_line = malloc(
                            strlen(statement->assignment->variable_name) + 30);
                        snprintf(
                            assignment_line,
                            strlen(statement->assignment->variable_name) + 30,
                            "    mov qword %s, rax",
                            statement->assignment->variable_name);
                        arrput(lines, assignment_line);
                    }
                }
            }
            break;
        }
        case C_STATEMENT_NOOP: {
            break;
        }
        default:
            arrfree(lines);
            EXIT_WITH_ERROR("Got unsupported type for statement emit: %d\n",
                            statement->type);
    }

    return lines;
}

char **c_code_gen_emit_block(c_ast_block *block, int *current_offset) {
    char **lines = NULL;

    for (int i = 0; i < arrlen(block->statements); i++) {
        char **statement_lines =
            c_code_gen_emit_statement(block->statements[i], current_offset);

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

    int current_offset = 0;
    char **body_lines =
        c_code_gen_emit_block(function_declaration->body, &current_offset);
    ADD_TO_LINES(body_lines);
    arrfree(body_lines);

    arrput(lines, strdup(""));
    arrput(lines, strdup("    mov rsp, rbp"));
    arrput(lines, strdup("    pop rbp"));
    arrput(lines, strdup("    ret"));
    arrput(lines, strdup(""));

    return lines;
}
