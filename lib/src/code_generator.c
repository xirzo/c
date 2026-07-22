#include "code_generator.h"
#include <string.h>
#include "parser.h"
#include "stb_ds.h"
#include "str.h"
#include "utils.h"
#include "xi_string.h"

#define MAX_LITERAL_LENGTH 128

#define ADD_TO_LINES(new_lines)                 \
  for (int i = 0; i < arrlen(new_lines); i++) { \
    arrput(lines, new_lines[i]);                \
  }

char **C_CodeGenEmit(C_AstProgram *program) {
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
    char **function_lines =
        C_CodeGenEmitFunctionDeclaration(program->function_declarations[i]);

    for (int i = 0; i < arrlen(function_lines); i++) {
      arrput(lines, function_lines[i]);
    }

    arrfree(function_lines);
  }

  return lines;
}

char **C_CodeGenEmitConstant(C_AstConstant *constant) {
  char **lines = NULL;

  char line[MAX_LITERAL_LENGTH];
  snprintf(line, sizeof(line), "    mov rax, %d", constant->value);
  arrput(lines, strdup(line));

  return lines;
}

char **C_CodeGenEmitFunctionCall(C_AstFunctionCall *function_call,
                                 const char        *assign_to_variable) {
  char **lines = NULL;

  char function_label[MAX_LITERAL_LENGTH];
  snprintf(function_label, sizeof(function_label), "    call %s",
           StringGetCstr(&function_call->function_name));
  arrput(lines, strdup(function_label));

  if (assign_to_variable) {
    char assignment_line[MAX_LITERAL_LENGTH];
    snprintf(assignment_line, sizeof(assignment_line), "    mov qword %s, rax",
             assign_to_variable);
    arrput(lines, strdup(assignment_line));
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

char **C_CodeGenEmitVariableAssignment(C_AstVariableAssignment *assignment,
                                       int *current_offset) {
  char **lines = NULL;

  arrput(lines, strdup(""));
  // TODO: get size of the variable type
  arrput(lines, strdup("    sub rsp, 8"));

  *current_offset += 8;

  char variable_label[MAX_LITERAL_LENGTH];
  snprintf(variable_label, sizeof(variable_label), "    %%define %s [rbp-%d]",
           StringGetCstr(&assignment->variable_name), *current_offset);
  arrput(lines, strdup(variable_label));
  arrput(lines, strdup(""));

  return lines;
}

char **C_CodeGenEmitExpression(C_AstExpression *expression,
                               int             *current_offset) {
  char **lines = NULL;

  switch (expression->type) {
    case C_CONSTANT: {
      char **constant_lines = C_CodeGenEmitConstant(expression->constant);
      ADD_TO_LINES(constant_lines);
      arrfree(constant_lines);
      break;
    }
    case C_FUNCTION_CALL: {
      char **function_call_lines =
          C_CodeGenEmitFunctionCall(expression->function_call, NULL);
      ADD_TO_LINES(function_call_lines);
      arrfree(function_call_lines);
      break;
    }
    case C_VARIABLE: {
      char **variable_lines = C_CodeGenEmitVariable(expression->variable);
      ADD_TO_LINES(variable_lines);
      arrfree(variable_lines);
      break;
    }
    case C_BINARY_EXPRESSION: {
      char **binary_expression_lines =
          C_CodeGenEmitBinaryExpression(expression->binary, current_offset);
      ADD_TO_LINES(binary_expression_lines);
      arrfree(binary_expression_lines);
      break;
    }
    default:
      arrfree(lines);
      EXIT_WITH_ERROR("Got unsupported type for expression emit: %d\n",
                      expression->type);
  }

  return lines;
}

char **C_CodeGenEmitBinaryExpression(C_AstBinaryExpression *binary,
                                     int                   *current_offset) {
  char **lines = NULL;

  char **lhs_lines = C_CodeGenEmitExpression(binary->lhs, current_offset);
  ADD_TO_LINES(lhs_lines);
  arrfree(lhs_lines);

  arrput(lines, strdup("    push rax"));

  char **rhs_lines = C_CodeGenEmitExpression(binary->rhs, current_offset);
  ADD_TO_LINES(rhs_lines);
  arrfree(rhs_lines);

  arrput(lines, strdup("    pop rbx"));

  switch (binary->symbol) {
    case '+': {
      arrput(lines, strdup("    add rax, rbx"));
      break;
    }
    case '-': {
      arrput(lines, strdup("    sub rbx, rax"));
      arrput(lines, strdup("    mov rax, rbx"));
      break;
    }
    case '*': {
      arrput(lines, strdup("    imul rax, rbx"));
      break;
    }
    case '/': {
      arrput(lines, strdup("    mov rdx, 0"));
      arrput(lines, strdup("    mov rcx, rax"));
      arrput(lines, strdup("    mov rax, rbx"));
      // NOTE: remainder in rdx
      arrput(lines, strdup("    idiv rcx"));
      break;
    }
    default:
      EXIT_WITH_ERROR("Received inproper binary operator symbol: %c",
                      binary->symbol);
  }

  return lines;
}

char **C_CodeGenEmitVariable(C_AstVariable *variable) {
  char **lines = NULL;

  char load_line[MAX_LITERAL_LENGTH];
  snprintf(load_line, sizeof(load_line), "    mov rax, qword %s",
           StringGetCstr(&variable->name));
  arrput(lines, strdup(load_line));

  return lines;
}

char **C_CodeGenEmitReturn(C_AstReturn *ret, int *current_offset) {
  char **lines = NULL;

  if (ret->value) {
    char **expression_lines =
        C_CodeGenEmitExpression(ret->value, current_offset);
    ADD_TO_LINES(expression_lines);
    arrfree(expression_lines);
  }

  return lines;
}

char **C_CodeGenEmitStatement(C_AstStatement *statement, int *current_offset) {
  char **lines = NULL;

  switch (statement->type) {
    case C_STATEMENT_BLOCK: {
      char **block_lines = C_CodeGenEmitBlock(statement->block, current_offset);
      ADD_TO_LINES(block_lines);
      arrfree(block_lines);
      break;
    }
    case C_STATEMENT_RETURN: {
      char **return_lines =
          C_CodeGenEmitReturn(statement->return_statement, current_offset);
      ADD_TO_LINES(return_lines);
      arrfree(return_lines);
      break;
    }
    case C_STATEMENT_FUNCTION_DECLARATION: {
      char **function_declaration_lines =
          C_CodeGenEmitFunctionDeclaration(statement->function_declaration);
      ADD_TO_LINES(function_declaration_lines);
      arrfree(function_declaration_lines);
      break;
    }
    case C_STATEMENT_EXPRESSION: {
      char **expression_lines =
          C_CodeGenEmitExpression(statement->expression, current_offset);
      ADD_TO_LINES(expression_lines);
      arrfree(expression_lines);
      break;
    }
    case C_STATEMENT_ASSIGNMENT: {
      char **assignment_lines = C_CodeGenEmitVariableAssignment(
          statement->assignment, current_offset);
      ADD_TO_LINES(assignment_lines);
      arrfree(assignment_lines);

      if (statement->assignment->expression) {
        switch (statement->assignment->expression->type) {
          case C_FUNCTION_CALL: {
            char **function_call_lines = C_CodeGenEmitFunctionCall(
                statement->assignment->expression->function_call,
                StringGetCstr(&statement->assignment->variable_name));
            ADD_TO_LINES(function_call_lines);
            arrfree(function_call_lines);
            break;
          }
          default: {
            char **expression_lines = C_CodeGenEmitExpression(
                statement->assignment->expression, current_offset);
            ADD_TO_LINES(expression_lines);
            arrfree(expression_lines);

            char assignment_line[MAX_LITERAL_LENGTH];
            snprintf(assignment_line, sizeof(assignment_line),
                     "    mov qword %s, rax",
                     StringGetCstr(&statement->assignment->variable_name));
            arrput(lines, strdup(assignment_line));
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

char **C_CodeGenEmitBlock(C_AstBlock *block, int *current_offset) {
  char **lines = NULL;

  for (int i = 0; i < arrlen(block->statements); i++) {
    char **statement_lines =
        C_CodeGenEmitStatement(block->statements[i], current_offset);

    ADD_TO_LINES(statement_lines);
    arrfree(statement_lines);
  }

  return lines;
}

char **C_CodeGenEmitFunctionDeclaration(
    C_AstFunctionDeclaration *function_declaration) {
  char **lines = NULL;

  char function_label[MAX_LITERAL_LENGTH];
  snprintf(function_label, sizeof(function_label),
           "%s:", StringGetCstr(&function_declaration->function_name));
  arrput(lines, strdup(function_label));

  arrput(lines, strdup("    push rbp"));
  arrput(lines, strdup("    mov rbp, rsp"));

  int    current_offset = 0;
  char **body_lines =
      C_CodeGenEmitBlock(function_declaration->body, &current_offset);
  ADD_TO_LINES(body_lines);
  arrfree(body_lines);

  arrput(lines, strdup(""));
  arrput(lines, strdup("    mov rsp, rbp"));
  arrput(lines, strdup("    pop rbp"));
  arrput(lines, strdup("    ret"));
  arrput(lines, strdup(""));

  return lines;
}
