#include "parser.h"
#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include "error.h"
#include "lexer.h"
#include "stb_ds.h"
#include "utils.h"
#include "xi_string.h"

C_Parser *C_ParserCreate(C_Token *tokens, C_ErrorContext *error_context,
                         const char *filename) {
  if (arrlen(tokens) == 0) {
    C_ErrorReport(error_context, "Got empty token list", filename, 1, 1);
    return NULL;
  }

  C_Parser *parser = malloc(sizeof(C_Parser));

  parser->current_position = 0;
  parser->read_position    = 1;
  parser->current_token    = tokens[0];
  parser->tokens           = tokens;
  parser->error_context    = error_context;
  parser->filename         = filename;

  return parser;
}

void C_ParserAdvance(C_Parser *parser) {
  size_t tokens_len = arrlenu(parser->tokens);

  if (parser->read_position >= tokens_len) {
    parser->current_token.type = C_EOF;
    parser->current_position   = tokens_len;
    return;
  }

  parser->current_token    = parser->tokens[parser->read_position];
  parser->current_position = parser->read_position;
  parser->read_position++;
}

C_Token C_ParserPeek(C_Parser *parser) {
  size_t tokens_len = arrlenu(parser->tokens);

  if (parser->read_position >= tokens_len) {
    C_Token eof_token = {0};
    eof_token.type    = C_EOF;
    return eof_token;
  }

  return parser->tokens[parser->read_position];
}

C_Token C_ParserPeekAhead(C_Parser *parser) {
  size_t tokens_len = arrlenu(parser->tokens);

  if (parser->read_position + 1 >= tokens_len) {
    C_Token eof_token = {0};
    eof_token.type    = C_EOF;
    return eof_token;
  }

  return parser->tokens[parser->read_position + 1];
}

C_AstConstant *C_ParserParseConstant(C_Parser *parser) {
  C_AstConstant *constant = malloc(sizeof(C_AstConstant));
  constant->type = C_AST_CONSTANT_INT;
  constant->value.int_value = atoi(StringGetCstr(&parser->current_token.string));
  C_ParserAdvance(parser);
  return constant;
}

C_AstFunctionCall *C_ParserParseFunctionCall(C_Parser *parser) {
  C_AstFunctionCall *function_call = malloc(sizeof(C_AstFunctionCall));
  function_call->function_name = StringDuplicate(&parser->current_token.string);

  LOG_DEBUG("Parsing function call\n");
  C_ParserAdvance(parser);

  if (parser->current_token.type != C_LPAREN) {
    C_ErrorReportWithToken(parser->error_context,
                           "Expected '(' after function name",
                           parser->current_token, parser->filename);
    StringFree(&function_call->function_name);
    free(function_call);
    return NULL;
  }

  C_ParserAdvance(parser);

  if (parser->current_token.type != C_RPAREN) {
    C_ErrorReportWithToken(parser->error_context,
                           "Expected ')' after function call",
                           parser->current_token, parser->filename);
    StringFree(&function_call->function_name);
    free(function_call);
    return NULL;
  }

  C_ParserAdvance(parser);
  return function_call;
}

C_AstStatement *C_ParserParseStatement(C_Parser *parser) {
  C_AstStatement *statement = malloc(sizeof(C_AstStatement));

  LOG_DEBUG("Parsing statement\n");

  switch (parser->current_token.type) {
    case C_INTEGER:
      if (C_ParserPeekAhead(parser).type == C_LPAREN) {
        statement->type = C_STATEMENT_FUNCTION_DECLARATION;
        statement->function_declaration =
            C_ParserParseFunctionDeclaration(parser);
        if (!statement->function_declaration) {
          free(statement);
          return NULL;
        }
        break;
      }

      statement->type       = C_STATEMENT_ASSIGNMENT;
      statement->assignment = C_ParserParseVariableAssignment(parser);
      if (!statement->assignment) {
        free(statement);
        return NULL;
      }
      break;
    case C_LBRACE:
      statement->type  = C_STATEMENT_BLOCK;
      statement->block = C_ParserParseBlock(parser);
      if (!statement->block) {
        free(statement);
        return NULL;
      }
      break;
    case C_RETURN:
      statement->type             = C_STATEMENT_RETURN;
      statement->return_statement = C_ParserParseReturn(parser);
      if (!statement->return_statement) {
        free(statement);
        return NULL;
      }
      break;
    case C_SEMICOLON:
      statement->type = C_STATEMENT_NOOP;
      C_ParserAdvance(parser);
      break;
    default:
      statement->type       = C_STATEMENT_EXPRESSION;
      statement->expression = C_ParserParseExpression(parser);
      if (!statement->expression) {
        free(statement);
        return NULL;
      }

      if (parser->current_token.type != C_SEMICOLON) {
        C_ErrorReportWithToken(parser->error_context,
                               "Expected ';' after expression",
                               parser->current_token, parser->filename);
        C_AstFreeExpression(statement->expression);
        free(statement);
        return NULL;
      }
      C_ParserAdvance(parser);
      break;
  }

  return statement;
}

C_AstVariableAssignment *C_ParserParseVariableAssignment(C_Parser *parser) {
  LOG_DEBUG("Parsing variable assignment\n");
  C_AstVariableAssignment *assignment = malloc(sizeof(C_AstVariableAssignment));

  C_ParserAdvance(parser);

  if (parser->current_token.type != C_IDENTIFIER) {
    C_ErrorReportWithToken(parser->error_context,
                           "Expected identifier after type",
                           parser->current_token, parser->filename);
    free(assignment);
    return NULL;
  }

  assignment->variable_name = StringDuplicate(&parser->current_token.string);

  C_ParserAdvance(parser);

  if (parser->current_token.type != C_ASSIGN) {
    C_ErrorReportWithToken(parser->error_context,
                           "Expected '=' after variable name",
                           parser->current_token, parser->filename);
    StringFree(&assignment->variable_name);
    free(assignment);
    return NULL;
  }

  C_ParserAdvance(parser);

  assignment->expression = C_ParserParseExpression(parser);
  if (!assignment->expression) {
    StringFree(&assignment->variable_name);
    free(assignment);
    return NULL;
  }

  if (parser->current_token.type != C_SEMICOLON) {
    C_ErrorReportWithToken(parser->error_context,
                           "Expected ';' after assignment",
                           parser->current_token, parser->filename);
    StringFree(&assignment->variable_name);
    C_AstFreeExpression(assignment->expression);
    free(assignment);
    return NULL;
  }
  C_ParserAdvance(parser);

  return assignment;
}

C_AstExpression *C_ParserParseExpression(C_Parser *parser) {
  return C_ParserParseExpressionWithPrecedence(parser, 0);
}

C_InfixBindingPower C_GetInfixBindingPower(C_TokenType token_type) {
  switch (token_type) {
    case C_PLUS:
    case C_MINUS:
      return (C_InfixBindingPower){.left = 1, .right = 2};
    case C_ASTERISK:
    case C_SLASH:
      return (C_InfixBindingPower){.left = 3, .right = 4};
    default:
      return (C_InfixBindingPower){.left = 0, .right = 0};
  }
}

C_AstExpression *C_ParserParseExpressionWithPrecedence(
    C_Parser *parser, double min_binding_power) {
  LOG_DEBUG("Parsing expression (Pratt Parsing)\n");

  C_AstExpression *lhs = malloc(sizeof(C_AstExpression));

  switch (parser->current_token.type) {
    case C_INTEGER_LITERAL: {
      lhs->type     = C_CONSTANT;
      lhs->constant = C_ParserParseConstant(parser);
      break;
    }

    case C_STRING_LITERAL: {
    }

    case C_IDENTIFIER: {
      if (C_ParserPeek(parser).type == C_LPAREN) {
        lhs->type          = C_FUNCTION_CALL;
        lhs->function_call = C_ParserParseFunctionCall(parser);
        if (!lhs->function_call) {
          free(lhs);
          return NULL;
        }
        break;
      }

      lhs->type     = C_VARIABLE;
      lhs->variable = C_ParserParseVariable(parser);
      break;
    }

    default:
      C_ErrorReportWithToken(parser->error_context, "Expected expression",
                             parser->current_token, parser->filename);
      free(lhs);
      return NULL;
  }

  while (1) {
    C_TokenType operator_type = parser->current_token.type;

    switch (operator_type) {
      case C_PLUS:
      case C_MINUS:
      case C_ASTERISK:
      case C_SLASH:
        break;
      case C_EOF:
        return lhs;
      default:
        return lhs;
    }

    C_InfixBindingPower power = C_GetInfixBindingPower(operator_type);

    if (power.left < min_binding_power) {
      break;
    }

    C_ParserAdvance(parser);

    C_AstExpression *rhs =
        C_ParserParseExpressionWithPrecedence(parser, power.right);
    if (!rhs) {
      C_AstFreeExpression(lhs);
      return NULL;
    }

    C_AstExpression *binary_expr = malloc(sizeof(C_AstExpression));
    binary_expr->type            = C_BINARY_EXPRESSION;
    binary_expr->binary          = malloc(sizeof(C_AstBinaryExpression));

    switch (operator_type) {
      case C_PLUS:
        binary_expr->binary->symbol = '+';
        break;
      case C_MINUS:
        binary_expr->binary->symbol = '-';
        break;
      case C_ASTERISK:
        binary_expr->binary->symbol = '*';
        break;
      case C_SLASH:
        binary_expr->binary->symbol = '/';
        break;
      default:
        binary_expr->binary->symbol = '?';
        break;
    }

    binary_expr->binary->lhs = lhs;
    binary_expr->binary->rhs = rhs;
    lhs                      = binary_expr;
  }

  return lhs;
}

C_AstVariable *C_ParserParseVariable(C_Parser *parser) {
  C_AstVariable *variable = malloc(sizeof(C_AstVariable));

  LOG_DEBUG("Parsing variable\n");

  if (parser->current_token.type != C_IDENTIFIER) {
    C_ErrorReportWithToken(parser->error_context, "Expected identifier",
                           parser->current_token, parser->filename);
    free(variable);
    return NULL;
  }

  variable->name = StringDuplicate(&parser->current_token.string);
  C_ParserAdvance(parser);

  return variable;
}

C_AstReturn *C_ParserParseReturn(C_Parser *parser) {
  C_AstReturn *return_statement = malloc(sizeof(C_AstReturn));

  LOG_DEBUG("Parsing return\n");

  if (parser->current_token.type != C_RETURN) {
    C_ErrorReportWithToken(parser->error_context, "Expected 'return' keyword",
                           parser->current_token, parser->filename);
    free(return_statement);
    return NULL;
  }

  C_ParserAdvance(parser);

  return_statement->value = C_ParserParseExpression(parser);
  if (!return_statement->value) {
    free(return_statement);
    return NULL;
  }

  if (parser->current_token.type != C_SEMICOLON) {
    C_ErrorReportWithToken(parser->error_context,
                           "Expected ';' after return statement",
                           parser->current_token, parser->filename);
    C_AstFreeExpression(return_statement->value);
    free(return_statement);
    return NULL;
  }
  C_ParserAdvance(parser);

  return return_statement;
}

C_AstBlock *C_ParserParseBlock(C_Parser *parser) {
  C_AstBlock *block = malloc(sizeof(C_AstBlock));
  block->statements = NULL;

  LOG_DEBUG("Parsing block\n");

  if (parser->current_token.type != C_LBRACE) {
    C_ErrorReportWithToken(parser->error_context, "Expected '{' to start block",
                           parser->current_token, parser->filename);
    free(block);
    return NULL;
  }

  C_ParserAdvance(parser);

  while (parser->current_token.type != C_RBRACE &&
         parser->current_token.type != C_EOF) {
    C_AstStatement *statement = C_ParserParseStatement(parser);
    if (statement) {
      arrput(block->statements, statement);
    } else {
      C_ParserSynchronize(parser);
    }
  }

  if (parser->current_token.type != C_RBRACE) {
    C_ErrorReportWithToken(parser->error_context, "Expected '}' to end block",
                           parser->current_token, parser->filename);
  } else {
    C_ParserAdvance(parser);
  }

  return block;
}

C_AstFunctionDeclaration *C_ParserParseFunctionDeclaration(C_Parser *parser) {
  LOG_DEBUG("Parsing function declaration\n");
  C_AstFunctionDeclaration *function_declaration =
      malloc(sizeof(C_AstFunctionDeclaration));
  function_declaration->body = NULL;

  C_ParserAdvance(parser);

  if (parser->current_token.type != C_IDENTIFIER) {
    C_ErrorReportWithToken(parser->error_context,
                           "Expected function name after type",
                           parser->current_token, parser->filename);
    free(function_declaration);
    return NULL;
  }

  function_declaration->function_name =
      StringDuplicate(&parser->current_token.string);

  C_ParserAdvance(parser);

  if (parser->current_token.type != C_LPAREN) {
    C_ErrorReportWithToken(parser->error_context,
                           "Expected '(' after function name",
                           parser->current_token, parser->filename);
    StringFree(&function_declaration->function_name);
    free(function_declaration);
    return NULL;
  }

  C_ParserAdvance(parser);

  if (parser->current_token.type != C_RPAREN) {
    C_ErrorReportWithToken(parser->error_context,
                           "Expected ')' after function parameters",
                           parser->current_token, parser->filename);
    StringFree(&function_declaration->function_name);
    free(function_declaration);
    return NULL;
  }

  C_ParserAdvance(parser);

  function_declaration->body = C_ParserParseBlock(parser);
  if (!function_declaration->body) {
    StringFree(&function_declaration->function_name);
    free(function_declaration);
    return NULL;
  }

  return function_declaration;
}

C_AstProgram *C_ParserParse(C_Parser *parser) {
  C_AstProgram *program          = malloc(sizeof(C_AstProgram));
  program->function_declarations = NULL;

  while (parser->current_token.type != C_EOF) {
    switch (parser->current_token.type) {
      case C_INTEGER: {
        C_AstFunctionDeclaration *decl =
            C_ParserParseFunctionDeclaration(parser);
        if (decl) {
          arrput(program->function_declarations, decl);
        } else {
          C_ParserSynchronizeToDeclaration(parser);
        }
      } break;
      case C_EOF:
        goto done_parsing;
      default:
        C_ErrorReportWithToken(
            parser->error_context,
            "Unexpected token - expected function declaration",
            parser->current_token, parser->filename);
        C_ParserAdvance(parser);
        break;
    }
  }

done_parsing:
  return program;
}

void C_ParserSynchronize(C_Parser *parser) {
  while (parser->current_token.type != C_EOF) {
    switch (parser->current_token.type) {
      case C_SEMICOLON:
      case C_RBRACE:
      case C_LBRACE:
      case C_RETURN:
      case C_INTEGER:
        return;
      default:
        C_ParserAdvance(parser);
        break;
    }
  }
}

void C_ParserSynchronizeToDeclaration(C_Parser *parser) {
  while (parser->current_token.type != C_EOF) {
    if (parser->current_token.type == C_INTEGER &&
        C_ParserPeek(parser).type == C_IDENTIFIER &&
        C_ParserPeekAhead(parser).type == C_LPAREN) {
      return;
    }

    C_ParserAdvance(parser);
  }
}

void C_ParserFree(C_Parser *parser) {
  if (!parser) {
    return;
  }

  C_LexerFreeTokens(parser->tokens);
  free(parser);
}

void C_AstFreeExpression(C_AstExpression *expression) {
  if (!expression) {
    return;
  }

  switch (expression->type) {
    case C_CONSTANT:
      free(expression->constant);
      break;
    case C_FUNCTION_CALL:
      StringFree(&expression->function_call->function_name);
      free(expression->function_call);
      break;
    case C_BINARY_EXPRESSION:
      C_AstFreeExpression(expression->binary->lhs);
      C_AstFreeExpression(expression->binary->rhs);
      free(expression->binary);
      break;
    case C_VARIABLE:
      C_AstFreeVariable(expression->variable);
      break;
    default:
      EXIT_WITH_ERROR("Got unknown expression to free: %d\n", expression->type);
  }

  free(expression);
}

void C_AstFreeBlock(C_AstBlock *block) {
  if (!block) {
    return;
  }

  for (int i = 0; i < arrlen(block->statements); i++) {
    C_AstFreeStatement(block->statements[i]);
  }

  arrfree(block->statements);
  free(block);
}

void C_AstFreeReturn(C_AstReturn *ret) {
  if (!ret) {
    return;
  }

  C_AstFreeExpression(ret->value);
  free(ret);
}

void C_AstFreeVariableAssignment(C_AstVariableAssignment *assignment) {
  if (!assignment) {
    return;
  }

  StringFree(&assignment->variable_name);
  if (assignment->expression) {
    C_AstFreeExpression(assignment->expression);
  }
  free(assignment);
}

void C_AstFreeFunctionDeclaration(C_AstFunctionDeclaration *declaration) {
  if (!declaration) {
    return;
  }

  StringFree(&declaration->function_name);
  C_AstFreeBlock(declaration->body);
  free(declaration);
}

void C_AstFreeExpressionStatement(C_AstExpression *expression) {
  if (!expression) {
    return;
  }

  C_AstFreeExpression(expression);
}

void C_AstFreeStatement(C_AstStatement *statement) {
  if (!statement) {
    return;
  }

  switch (statement->type) {
    case C_STATEMENT_BLOCK:
      C_AstFreeBlock(statement->block);
      break;
    case C_STATEMENT_RETURN:
      C_AstFreeReturn(statement->return_statement);
      break;
    case C_STATEMENT_FUNCTION_DECLARATION:
      C_AstFreeFunctionDeclaration(statement->function_declaration);
      break;
    case C_STATEMENT_EXPRESSION:
      C_AstFreeExpressionStatement(statement->expression);
      break;
    case C_STATEMENT_ASSIGNMENT:
      C_AstFreeVariableAssignment(statement->assignment);
      break;
    case C_STATEMENT_NOOP:
      break;
    default:
      EXIT_WITH_ERROR("Got unknown statement to free: %d\n", statement->type);
  }

  free(statement);
}

void C_ParserFreeProgram(C_AstProgram *program) {
  if (!program) {
    return;
  }

  if (program->function_declarations) {
    for (int i = 0; i < arrlen(program->function_declarations); i++) {
      C_AstFreeFunctionDeclaration(program->function_declarations[i]);
    }

    arrfree(program->function_declarations);
  }

  free(program);
}

void C_AstFreeVariable(C_AstVariable *variable) {
  if (!variable) {
    return;
  }

  StringFree(&variable->name);
  free(variable);
}
