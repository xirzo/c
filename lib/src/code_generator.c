#include "code_generator.h"
#include <string.h>
#include "parser.h"
#include "stb_ds.h"
#include "str.h"
#include "utils.h"
#include "xi_string.h"

#define MAX_LITERAL_LENGTH 256

#define ADD_TO_LINES(new_lines)                 \
  for (int i = 0; i < arrlen(new_lines); i++) { \
    arrput(lines, new_lines[i]);                \
  }

static String *cg_string_data = NULL;
static int     cg_string_count = 0;

static void C_CodeGenResetStringState(void) {
  if (cg_string_data) {
    for (int i = 0; i < arrlen(cg_string_data); i++) {
      StringFree(&cg_string_data[i]);
    }
    arrfree(cg_string_data);
  }
  cg_string_data = NULL;
  cg_string_count = 0;
}

static void C_CodeGenEscapeString(const char *input, char *output,
                                  size_t output_size) {
  size_t j = 0;
  for (size_t i = 0; input[i] != '\0' && j + 6 < output_size; i++) {
    switch (input[i]) {
      case '\\':
        output[j++] = '\\';
        output[j++] = '\\';
        break;
      case '"':
        output[j++] = '\\';
        output[j++] = '"';
        break;
      case '\n':
        output[j++] = '\\';
        output[j++] = 'n';
        break;
      case '\t':
        output[j++] = '\\';
        output[j++] = 't';
        break;
      case '\r':
        output[j++] = '\\';
        output[j++] = 'r';
        break;
      default:
        if ((unsigned char)input[i] < 32) {
          j += snprintf(output + j, output_size - j, "\\x%02x",
                        (unsigned char)input[i]);
        } else {
          output[j++] = input[i];
        }
        break;
    }
  }
  output[j] = '\0';
}

static void C_CodeGenAddStringData(const char *label, const String *str) {
  char escaped[MAX_LITERAL_LENGTH];
  C_CodeGenEscapeString(StringGetCstr(str), escaped, sizeof(escaped));
  char line[MAX_LITERAL_LENGTH * 2];
  snprintf(line, sizeof(line), "%s: db \"%s\", 0", label, escaped);
  arrput(cg_string_data, StringCreate(line));
}

String *C_CodeGenEmit(C_AstProgram *program) {
  C_CodeGenResetStringState();
  String *lines = NULL;

  arrput(lines, StringCreate("global _start"));
  arrput(lines, StringCreate(""));
  arrput(lines, StringCreate("section .text"));
  arrput(lines, StringCreate("_start:"));
  arrput(lines, StringCreate("    call main"));
  arrput(lines, StringCreate(""));
  arrput(lines, StringCreate("    mov rdi, rax"));
  arrput(lines, StringCreate("    mov rax, 60"));
  arrput(lines, StringCreate("    syscall"));
  arrput(lines, StringCreate(""));

  for (int i = 0; i < arrlen(program->function_declarations); i++) {
    String *function_lines =
        C_CodeGenEmitFunctionDeclaration(program->function_declarations[i]);

    for (int i = 0; i < arrlen(function_lines); i++) {
      arrput(lines, function_lines[i]);
    }

    arrfree(function_lines);
  }

  if (arrlen(cg_string_data) > 0) {
    arrput(lines, StringCreate(""));
    arrput(lines, StringCreate("section .data"));
    for (int i = 0; i < arrlen(cg_string_data); i++) {
      arrput(lines, cg_string_data[i]);
    }
  }

  arrfree(cg_string_data);
  cg_string_data = NULL;
  cg_string_count = 0;

  return lines;
}

String *C_CodeGenEmitConstant(C_AstConstant *constant) {
  String *lines = NULL;

  if (constant->type == C_AST_CONSTANT_STRING) {
    char label[MAX_LITERAL_LENGTH];
    snprintf(label, sizeof(label), "str_%d", cg_string_count++);

    String code_line = StringCreateEmpty(0);
    StringPrintf(&code_line, "    lea rax, [rel %s]", label);
    arrput(lines, code_line);

    C_CodeGenAddStringData(label, &constant->value.string_value);
  } else {
    String line = StringCreateEmpty(0);
    StringPrintf(&line, "    mov rax, %d", constant->value.int_value);
    arrput(lines, line);
  }

  return lines;
}

String *C_CodeGenEmitFunctionCall(C_AstFunctionCall *function_call,
                                  const String      *assign_to_variable) {
  String *lines = NULL;

  String function_label = StringCreateEmpty(0);
  StringPrintf(&function_label, "    call %s",
               StringGetCstr(&function_call->function_name));
  arrput(lines, function_label);

  if (assign_to_variable) {
    String assignment_line = StringCreateEmpty(0);
    StringPrintf(&assignment_line, "    mov qword %s, rax",
                 StringGetCstr(assign_to_variable));
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

String *C_CodeGenEmitVariableAssignment(C_AstVariableAssignment *assignment,
                                        int *current_offset) {
  String *lines = NULL;

  arrput(lines, StringCreate(""));
  // TODO: get size of the variable type
  arrput(lines, StringCreate("    sub rsp, 8"));

  *current_offset += 8;

  String variable_label = StringCreateEmpty(0);
  StringPrintf(&variable_label, "    %%define %s [rbp-%d]",
               StringGetCstr(&assignment->variable_name), *current_offset);
  arrput(lines, variable_label);
  arrput(lines, StringCreate(""));

  return lines;
}

String *C_CodeGenEmitExpression(C_AstExpression *expression,
                                int             *current_offset) {
  String *lines = NULL;

  switch (expression->type) {
    case C_CONSTANT: {
      String *constant_lines = C_CodeGenEmitConstant(expression->constant);
      ADD_TO_LINES(constant_lines);
      arrfree(constant_lines);
      break;
    }
    case C_FUNCTION_CALL: {
      String *function_call_lines =
          C_CodeGenEmitFunctionCall(expression->function_call, NULL);
      ADD_TO_LINES(function_call_lines);
      arrfree(function_call_lines);
      break;
    }
    case C_VARIABLE: {
      String *variable_lines = C_CodeGenEmitVariable(expression->variable);
      ADD_TO_LINES(variable_lines);
      arrfree(variable_lines);
      break;
    }
    case C_BINARY_EXPRESSION: {
      String *binary_expression_lines =
          C_CodeGenEmitBinaryExpression(expression->binary, current_offset);
      ADD_TO_LINES(binary_expression_lines);
      arrfree(binary_expression_lines);
      break;
    }
    case C_UNARY_EXPRESSION: {
      String *unary_lines =
          C_CodeGenEmitUnaryExpression(expression->unary, current_offset);
      ADD_TO_LINES(unary_lines);
      arrfree(unary_lines);
      break;
    }
    default:
      arrfree(lines);
      EXIT_WITH_ERROR("Got unsupported type for expression emit: %d\n",
                      expression->type);
  }

  return lines;
}

String *C_CodeGenEmitBinaryExpression(C_AstBinaryExpression *binary,
                                      int                   *current_offset) {
  String *lines = NULL;

  String *lhs_lines = C_CodeGenEmitExpression(binary->lhs, current_offset);
  ADD_TO_LINES(lhs_lines);
  arrfree(lhs_lines);

  arrput(lines, StringCreate("    push rax"));

  String *rhs_lines = C_CodeGenEmitExpression(binary->rhs, current_offset);
  ADD_TO_LINES(rhs_lines);
  arrfree(rhs_lines);

  arrput(lines, StringCreate("    pop rbx"));

  switch (binary->symbol) {
    case '+':
      arrput(lines, StringCreate("    add rax, rbx"));
      break;
    case '-':
      arrput(lines, StringCreate("    sub rbx, rax"));
      arrput(lines, StringCreate("    mov rax, rbx"));
      break;
    case '*':
      arrput(lines, StringCreate("    imul rax, rbx"));
      break;
    case '/':
      arrput(lines, StringCreate("    mov rdx, 0"));
      arrput(lines, StringCreate("    mov rcx, rax"));
      arrput(lines, StringCreate("    mov rax, rbx"));
      // NOTE: remainder in rdx
      arrput(lines, StringCreate("    idiv rcx"));
      break;
    default:
      EXIT_WITH_ERROR("Received inproper binary operator symbol: %c",
                      binary->symbol);
  }

  return lines;
}

String *C_CodeGenEmitVariable(C_AstVariable *variable) {
  String *lines = NULL;

  String load_line = StringCreateEmpty(0);
  StringPrintf(&load_line, "    mov rax, qword %s",
               StringGetCstr(&variable->name));
  arrput(lines, load_line);

  return lines;
}

String *C_CodeGenEmitUnaryExpression(C_AstUnaryExpression *unary,
                                     int                  *current_offset) {
  String *lines = NULL;

  switch (unary->type) {
    case C_UNARY_DEREF: {
      String *operand_lines =
          C_CodeGenEmitExpression(unary->operand, current_offset);
      ADD_TO_LINES(operand_lines);
      arrfree(operand_lines);
      arrput(lines, StringCreate("    mov rax, qword [rax]"));
      break;
    }
    case C_UNARY_ADDRESS_OF: {
      if (unary->operand->type != C_VARIABLE) {
        arrfree(lines);
        EXIT_WITH_ERROR("Address-of requires a variable operand");
      }
      String address_line = StringCreateEmpty(0);
      StringPrintf(&address_line, "    lea rax, %s",
                   StringGetCstr(&unary->operand->variable->name));
      arrput(lines, address_line);
      break;
    }
  }

  return lines;
}

String *C_CodeGenEmitReturn(C_AstReturn *ret, int *current_offset) {
  String *lines = NULL;

  if (ret->value) {
    String *expression_lines =
        C_CodeGenEmitExpression(ret->value, current_offset);
    ADD_TO_LINES(expression_lines);
    arrfree(expression_lines);
  }

  return lines;
}

String *C_CodeGenEmitStatement(C_AstStatement *statement, int *current_offset) {
  String *lines = NULL;

  switch (statement->type) {
    case C_STATEMENT_BLOCK: {
      String *block_lines = C_CodeGenEmitBlock(statement->block, current_offset);
      ADD_TO_LINES(block_lines);
      arrfree(block_lines);
      break;
    }
    case C_STATEMENT_RETURN: {
      String *return_lines =
          C_CodeGenEmitReturn(statement->return_statement, current_offset);
      ADD_TO_LINES(return_lines);
      arrfree(return_lines);
      break;
    }
    case C_STATEMENT_FUNCTION_DECLARATION: {
      String *function_declaration_lines =
          C_CodeGenEmitFunctionDeclaration(statement->function_declaration);
      ADD_TO_LINES(function_declaration_lines);
      arrfree(function_declaration_lines);
      break;
    }
    case C_STATEMENT_EXPRESSION: {
      String *expression_lines =
          C_CodeGenEmitExpression(statement->expression, current_offset);
      ADD_TO_LINES(expression_lines);
      arrfree(expression_lines);
      break;
    }
    case C_STATEMENT_ASSIGNMENT: {
      String *assignment_lines = C_CodeGenEmitVariableAssignment(
          statement->assignment, current_offset);
      ADD_TO_LINES(assignment_lines);
      arrfree(assignment_lines);

      if (statement->assignment->expression) {
        switch (statement->assignment->expression->type) {
          case C_FUNCTION_CALL: {
            String *function_call_lines = C_CodeGenEmitFunctionCall(
                statement->assignment->expression->function_call,
                &statement->assignment->variable_name);
            ADD_TO_LINES(function_call_lines);
            arrfree(function_call_lines);
            break;
          }
          default: {
            String *expression_lines = C_CodeGenEmitExpression(
                statement->assignment->expression, current_offset);
            ADD_TO_LINES(expression_lines);
            arrfree(expression_lines);

            String assignment_line = StringCreateEmpty(0);
            StringPrintf(&assignment_line, "    mov qword %s, rax",
                         StringGetCstr(&statement->assignment->variable_name));
            arrput(lines, assignment_line);
          }
        }
      }
      break;
    }
    case C_STATEMENT_NOOP:
      break;
    default:
      arrfree(lines);
      EXIT_WITH_ERROR("Got unsupported type for statement emit: %d\n",
                      statement->type);
  }

  return lines;
}

String *C_CodeGenEmitBlock(C_AstBlock *block, int *current_offset) {
  String *lines = NULL;

  for (int i = 0; i < arrlen(block->statements); i++) {
    String *statement_lines =
        C_CodeGenEmitStatement(block->statements[i], current_offset);

    ADD_TO_LINES(statement_lines);
    arrfree(statement_lines);
  }

  return lines;
}

String *C_CodeGenEmitFunctionDeclaration(
    C_AstFunctionDeclaration *function_declaration) {
  String *lines = NULL;

  String function_label = StringCreateEmpty(0);
  StringPrintf(&function_label, "%s:",
               StringGetCstr(&function_declaration->function_name));
  arrput(lines, function_label);

  arrput(lines, StringCreate("    push rbp"));
  arrput(lines, StringCreate("    mov rbp, rsp"));

  int     current_offset = 0;
  String *body_lines =
      C_CodeGenEmitBlock(function_declaration->body, &current_offset);
  ADD_TO_LINES(body_lines);
  arrfree(body_lines);

  arrput(lines, StringCreate(""));
  arrput(lines, StringCreate("    mov rsp, rbp"));
  arrput(lines, StringCreate("    pop rbp"));
  arrput(lines, StringCreate("    ret"));
  arrput(lines, StringCreate(""));

  return lines;
}
