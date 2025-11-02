#ifndef CODE_GENERATOR
#define CODE_GENERATOR

#include "parser.h"

// https://metanit.com/assembler/nasm/1.4.php
//
// global _start           ; делаем метку метку _start видимой извне
//
// section .text           ; объявление секции кода
// _start:                 ; объявление метки _start - точки входа в программу
//     mov rax, 60         ; 60 - номер системного вызова exit
//     mov rdi, 22         ; произвольный код возврата - 22
//     syscall             ; выполняем системный вызов exit

char **c_code_gen_emit(c_ast_program *program);
char **c_code_gen_emit_statement(c_ast_statement *statement);
char **c_code_gen_emit_block(c_ast_block *block);
char **c_code_gen_emit_function_declaration(
    c_ast_function_declaration *function_declaration);
char **c_code_gen_emit_function_call(c_ast_function_call *function_call);

#endif  // !CODE_GENERATOR
