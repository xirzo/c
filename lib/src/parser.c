#include "parser.h"
#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static int C_ParseCharEscape(const char *str, size_t *offset) {
  if (str[*offset] != '\\') {
    return (unsigned char)str[(*offset)++];
  }

  (*offset)++;
  switch (str[*offset]) {
    case 'n':
      (*offset)++;
      return '\n';
    case 't':
      (*offset)++;
      return '\t';
    case 'r':
      (*offset)++;
      return '\r';
    case '0':
      (*offset)++;
      return '\0';
    case '\\':
      (*offset)++;
      return '\\';
    case '\'':
      (*offset)++;
      return '\'';
    case '"':
      (*offset)++;
      return '"';
    case 'x': {
      (*offset)++;
      int value = 0;
      for (int i = 0; i < 2 && isxdigit((unsigned char)str[*offset]); i++) {
        char c = str[*offset];
        if (isdigit((unsigned char)c))
          value = value * 16 + (c - '0');
        else if (c >= 'a' && c <= 'f')
          value = value * 16 + (c - 'a' + 10);
        else
          value = value * 16 + (c - 'A' + 10);
        (*offset)++;
      }
      return value;
    }
    default:
      return (unsigned char)str[(*offset)++];
  }
}

static int C_ParsePointerDepth(C_Parser *parser) {
  int depth = 0;
  while (parser->current_token.type == C_ASTERISK) {
    depth++;
    C_ParserAdvance(parser);
  }
  return depth;
}

static bool C_IsFunctionDeclaration(C_Parser *parser) {
  size_t pos = parser->current_position + 1;
  size_t len = arrlenu(parser->tokens);

  if (pos >= len) return false;

  while (pos < len && parser->tokens[pos].type == C_ASTERISK) {
    pos++;
  }

  if (pos >= len || parser->tokens[pos].type != C_IDENTIFIER) return false;
  pos++;

  return pos < len && parser->tokens[pos].type == C_LPAREN;
}

C_AstConstant *C_ParserParseConstant(C_Parser *parser) {
  C_AstConstant *constant = malloc(sizeof(C_AstConstant));

  switch (parser->current_token.type) {
    case C_INTEGER_LITERAL:
      constant->type = C_AST_CONSTANT_INT;
      constant->value.int_value =
          atoi(StringGetCstr(&parser->current_token.string));
      break;
    case C_CHAR_LITERAL: {
      constant->type            = C_AST_CONSTANT_CHAR;
      const char *raw           = StringGetCstr(&parser->current_token.string);
      size_t      offset        = 0;
      constant->value.int_value = C_ParseCharEscape(raw, &offset);
      break;
    }
    case C_STRING_LITERAL:
      constant->type               = C_AST_CONSTANT_STRING;
      constant->value.string_value = parser->current_token.string;
      break;

    default:
      free(constant);
      EXIT_WITH_ERROR("Cannot parse a constant from token, \"%s\"",
                      C_TokenTypeToString(parser->current_token.type));
  }

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

C_AstIf *C_ParserParseIf(C_Parser *parser) {
  C_AstIf *if_statement   = malloc(sizeof(C_AstIf));
  if_statement->condition = NULL;
  if_statement->block     = NULL;

  if_statement->has_else = false;

  LOG_DEBUG("Parsing if statement\n");
  C_ParserAdvance(parser);

  if (parser->current_token.type != C_LPAREN) {
    C_ErrorReportWithToken(parser->error_context,
                           "Expected '(' after if statement",
                           parser->current_token, parser->filename);
    free(if_statement);
    return NULL;
  }

  C_ParserAdvance(parser);

  if_statement->condition = C_ParserParseExpression(parser);
  if (!if_statement->condition) {
    LOG_DEBUG("Failed to parse condition for if statement");
    free(if_statement);
    return NULL;
  }

  if (parser->current_token.type != C_RPAREN) {
    C_ErrorReportWithToken(parser->error_context,
                           "Expected ')' after if statement",
                           parser->current_token, parser->filename);
    C_AstFreeExpression(&if_statement->condition);
    free(if_statement);
    return NULL;
  }

  C_ParserAdvance(parser);

  if_statement->block = C_ParserParseStatement(parser);
  if (!if_statement->block) {
    LOG_DEBUG("Failed to parse body for if statement");
    C_AstFreeExpression(&if_statement->condition);
    free(if_statement);
    return NULL;
  }

  if (parser->current_token.type == C_ELSE) {
    LOG_DEBUG("Parsing else branch of if statement\n");
    C_ParserAdvance(parser);
    if_statement->else_block = C_ParserParseStatement(parser);
    if_statement->has_else   = true;
  }

  return if_statement;
}

C_AstStatement *C_ParserParseStatement(C_Parser *parser) {
  C_AstStatement *statement = malloc(sizeof(C_AstStatement));

  LOG_DEBUG("Parsing statement\n");

  switch (parser->current_token.type) {
    case C_INTEGER:
    case C_CHAR:
    case C_VOID:
      if (C_IsFunctionDeclaration(parser)) {
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
    case C_IF:
      statement->type         = C_STATEMENT_IF;
      statement->if_statement = C_ParserParseIf(parser);
      if (!statement->if_statement) {
        free(statement);
        return NULL;
      }
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
        C_AstFreeExpression(&statement->expression);
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

  assignment->pointer_depth = C_ParsePointerDepth(parser);

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
    C_AstFreeExpression(&assignment->expression);
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
  LOG_DEBUG("Parsing expression with min_binding_power: %.1f\n",
            min_binding_power);
  LOG_DEBUG("Current token: %s\n",
            C_TokenTypeToString(parser->current_token.type));

  C_AstExpression *lhs = malloc(sizeof(C_AstExpression));
  if (!lhs) {
    LOG_DEBUG("ERROR: Failed to allocate lhs expression\n");
    return NULL;
  }
  LOG_DEBUG("Allocated lhs expression at %p\n", (void *)lhs);

  switch (parser->current_token.type) {
    case C_INTEGER_LITERAL: {
      LOG_DEBUG("Parsing integer literal\n");
      lhs->type     = C_CONSTANT;
      lhs->constant = C_ParserParseConstant(parser);
      LOG_DEBUG("Created integer constant at %p\n", (void *)lhs->constant);
      break;
    }

    case C_CHAR_LITERAL:
    case C_STRING_LITERAL: {
      LOG_DEBUG("Parsing char/string literal (type: %d)\n",
                parser->current_token.type);
      lhs->type     = C_CONSTANT;
      lhs->constant = C_ParserParseConstant(parser);
      LOG_DEBUG("Created string/char constant at %p\n", (void *)lhs->constant);
      break;
    }

    case C_ASTERISK: {
      LOG_DEBUG("Parsing dereference operator (*)\n");
      C_ParserAdvance(parser);
      LOG_DEBUG("Advanced past '*', parsing operand with precedence 5\n");

      C_AstExpression *operand =
          C_ParserParseExpressionWithPrecedence(parser, 5);
      if (!operand) {
        LOG_DEBUG("ERROR: Failed to parse dereference operand\n");
        free(lhs);
        return NULL;
      }
      LOG_DEBUG("Parsed dereference operand at %p\n", (void *)operand);

      lhs->type  = C_UNARY_EXPRESSION;
      lhs->unary = malloc(sizeof(C_AstUnaryExpression));
      if (!lhs->unary) {
        LOG_DEBUG("ERROR: Failed to allocate unary expression\n");
        C_AstFreeExpression(&operand);
        free(lhs);
        return NULL;
      }
      lhs->unary->type    = C_UNARY_DEREF;
      lhs->unary->operand = operand;
      LOG_DEBUG("Created dereference expression at %p with operand %p\n",
                (void *)lhs, (void *)operand);
      break;
    }

    case C_AMPERSAND: {
      LOG_DEBUG("Parsing address-of operator (&)\n");
      C_ParserAdvance(parser);
      LOG_DEBUG("Advanced past '&', parsing operand with precedence 5\n");

      C_AstExpression *operand =
          C_ParserParseExpressionWithPrecedence(parser, 5);
      if (!operand) {
        LOG_DEBUG("ERROR: Failed to parse address-of operand\n");
        free(lhs);
        return NULL;
      }
      LOG_DEBUG("Parsed address-of operand at %p\n", (void *)operand);

      lhs->type  = C_UNARY_EXPRESSION;
      lhs->unary = malloc(sizeof(C_AstUnaryExpression));
      if (!lhs->unary) {
        LOG_DEBUG("ERROR: Failed to allocate unary expression\n");
        C_AstFreeExpression(&operand);
        free(lhs);
        return NULL;
      }
      lhs->unary->type    = C_UNARY_ADDRESS_OF;
      lhs->unary->operand = operand;
      LOG_DEBUG("Created address-of expression at %p with operand %p\n",
                (void *)lhs, (void *)operand);
      break;
    }

    case C_IDENTIFIER: {
      LOG_DEBUG("Parsing identifier: %s\n",
                StringGetCstr(&parser->current_token.string));

      if (C_ParserPeek(parser).type == C_LPAREN) {
        LOG_DEBUG("Identifier followed by '(', parsing function call\n");
        lhs->type          = C_FUNCTION_CALL;
        lhs->function_call = C_ParserParseFunctionCall(parser);
        if (!lhs->function_call) {
          LOG_DEBUG("ERROR: Failed to parse function call\n");
          free(lhs);
          return NULL;
        }
        LOG_DEBUG(
            "Created function call expression at %p with function call %p\n",
            (void *)lhs, (void *)lhs->function_call);
        break;
      }

      LOG_DEBUG("Identifier is variable reference\n");
      lhs->type     = C_VARIABLE;
      lhs->variable = C_ParserParseVariable(parser);
      if (!lhs->variable) {
        LOG_DEBUG("ERROR: Failed to parse variable\n");
        free(lhs);
        return NULL;
      }
      LOG_DEBUG("Created variable expression at %p with variable %p\n",
                (void *)lhs, (void *)lhs->variable);
      break;
    }

    default: {
      LOG_DEBUG("ERROR: Unexpected token: %s (type: %d)\n",
                StringGetCstr(&parser->current_token.string),
                parser->current_token.type);
      C_ErrorReportWithToken(parser->error_context, "Expected expression",
                             parser->current_token, parser->filename);
      free(lhs);
      return NULL;
    }
  }

  LOG_DEBUG(
      "Initial LHS parsed, entering infix loop. Current token: %s (type: %d)\n",
      StringGetCstr(&parser->current_token.string), parser->current_token.type);

  while (1) {
    C_TokenType operator_type = parser->current_token.type;
    LOG_DEBUG("Loop iteration - operator_type: %d, token: %s\n", operator_type,
              StringGetCstr(&parser->current_token.string));

    switch (operator_type) {
      case C_PLUS:
      case C_MINUS:
      case C_ASTERISK:
      case C_SLASH:
        LOG_DEBUG("Found binary operator: %d\n", operator_type);
        break;
      case C_EOF:
        LOG_DEBUG("Reached EOF, returning lhs at %p\n", (void *)lhs);
        return lhs;
      default:
        LOG_DEBUG("No more operators (token: %d), returning lhs at %p\n",
                  operator_type, (void *)lhs);
        return lhs;
    }

    C_InfixBindingPower power = C_GetInfixBindingPower(operator_type);
    LOG_DEBUG("Operator binding power - left: %.1f, right: %.1f\n", power.left,
              power.right);

    if (power.left < min_binding_power) {
      LOG_DEBUG(
          "Binding power left (%.1f) < min_binding_power (%.1f), breaking\n",
          power.left, min_binding_power);
      break;
    }

    LOG_DEBUG("Advancing past operator\n");
    C_ParserAdvance(parser);
    LOG_DEBUG("Parsing RHS with right binding power: %.1f\n", power.right);

    C_AstExpression *rhs =
        C_ParserParseExpressionWithPrecedence(parser, power.right);
    if (!rhs) {
      LOG_DEBUG("ERROR: Failed to parse RHS expression\n");
      LOG_DEBUG("Freeing LHS at %p\n", (void *)lhs);
      C_AstFreeExpression(&lhs);
      LOG_DEBUG("LHS set to NULL after free\n");
      return NULL;
    }
    LOG_DEBUG("Parsed RHS at %p\n", (void *)rhs);

    C_AstExpression *binary_expr = malloc(sizeof(C_AstExpression));
    if (!binary_expr) {
      LOG_DEBUG("ERROR: Failed to allocate binary expression\n");
      C_AstFreeExpression(&lhs);
      C_AstFreeExpression(&rhs);
      return NULL;
    }
    LOG_DEBUG("Allocated binary expression at %p\n", (void *)binary_expr);

    binary_expr->type   = C_BINARY_EXPRESSION;
    binary_expr->binary = malloc(sizeof(C_AstBinaryExpression));
    if (!binary_expr->binary) {
      LOG_DEBUG("ERROR: Failed to allocate binary expression struct\n");
      free(binary_expr);
      C_AstFreeExpression(&lhs);
      C_AstFreeExpression(&rhs);
      return NULL;
    }
    LOG_DEBUG("Allocated binary expression struct at %p\n",
              (void *)binary_expr->binary);

    switch (operator_type) {
      case C_PLUS:
        binary_expr->binary->symbol = '+';
        LOG_DEBUG("Set binary operator: '+'\n");
        break;
      case C_MINUS:
        binary_expr->binary->symbol = '-';
        LOG_DEBUG("Set binary operator: '-'\n");
        break;
      case C_ASTERISK:
        binary_expr->binary->symbol = '*';
        LOG_DEBUG("Set binary operator: '*'\n");
        break;
      case C_SLASH:
        binary_expr->binary->symbol = '/';
        LOG_DEBUG("Set binary operator: '/'\n");
        break;
      default:
        binary_expr->binary->symbol = '?';
        LOG_DEBUG("Set binary operator: '?' (unknown)\n");
        break;
    }

    binary_expr->binary->lhs = lhs;
    binary_expr->binary->rhs = rhs;
    LOG_DEBUG("Binary expression - lhs: %p, rhs: %p, symbol: %c\n",
              (void *)binary_expr->binary->lhs,
              (void *)binary_expr->binary->rhs, binary_expr->binary->symbol);

    lhs = binary_expr;
    LOG_DEBUG("Set lhs to binary expression at %p\n", (void *)lhs);
  }

  LOG_DEBUG("Returning final lhs at %p\n", (void *)lhs);
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
    C_AstFreeExpression(&return_statement->value);
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

  C_ParsePointerDepth(parser);

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

  if (parser->current_token.type == C_SEMICOLON) {
    C_ParserAdvance(parser);
    function_declaration->body = NULL;
    return function_declaration;
  }

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
      case C_INTEGER:
      case C_CHAR:
      case C_VOID: {
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
      case C_CHAR:
      case C_VOID:
        return;
      default:
        C_ParserAdvance(parser);
        break;
    }
  }
}

void C_ParserSynchronizeToDeclaration(C_Parser *parser) {
  while (parser->current_token.type != C_EOF) {
    if ((parser->current_token.type == C_INTEGER ||
         parser->current_token.type == C_CHAR ||
         parser->current_token.type == C_VOID) &&
        C_ParserPeek(parser).type == C_IDENTIFIER &&
        C_ParserPeekAhead(parser).type == C_LPAREN) {
      return;
    }

    C_ParserAdvance(parser);
  }
}

void C_ParserFree(C_Parser **parser) {
  if (!parser || !*parser) {
    return;
  }

  C_LexerFreeTokens((*parser)->tokens);
  free(*parser);
  *parser = NULL;
}

void C_AstFreeExpression(C_AstExpression **expression) {
  if (!expression || !*expression) {
    return;
  }

  switch ((*expression)->type) {
    case C_CONSTANT:
      free((*expression)->constant);
      break;
    case C_FUNCTION_CALL:
      StringFree(&(*expression)->function_call->function_name);
      free((*expression)->function_call);
      break;
    case C_BINARY_EXPRESSION:
      C_AstFreeExpression(&(*expression)->binary->lhs);
      C_AstFreeExpression(&(*expression)->binary->rhs);
      free((*expression)->binary);
      break;
    case C_VARIABLE:
      C_AstFreeVariable(&(*expression)->variable);
      break;
    case C_UNARY_EXPRESSION:
      C_AstFreeExpression(&(*expression)->unary->operand);
      free((*expression)->unary);
      break;
    default:
      EXIT_WITH_ERROR("Got unknown expression to free: %d\n",
                      (*expression)->type);
  }

  free(*expression);
  *expression = NULL;
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

  C_AstFreeExpression(&ret->value);
  free(ret);
}

void C_AstFreeVariableAssignment(C_AstVariableAssignment *assignment) {
  if (!assignment) {
    return;
  }

  StringFree(&assignment->variable_name);
  if (assignment->expression) {
    C_AstFreeExpression(&assignment->expression);
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

void C_AstFreeExpressionStatement(C_AstExpression **expression) {
  if (!expression || !*expression) {
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
      statement->block = NULL;
      break;
    case C_STATEMENT_RETURN:
      C_AstFreeReturn(statement->return_statement);
      statement->return_statement = NULL;
      break;
    case C_STATEMENT_FUNCTION_DECLARATION:
      C_AstFreeFunctionDeclaration(statement->function_declaration);
      statement->function_declaration = NULL;
      break;
    case C_STATEMENT_EXPRESSION:
      C_AstFreeExpressionStatement(&statement->expression);
      break;
    case C_STATEMENT_ASSIGNMENT:
      C_AstFreeVariableAssignment(statement->assignment);
      break;
    case C_STATEMENT_IF:
      C_AstFreeIf(&statement->if_statement);
      break;
    case C_STATEMENT_NOOP:
      break;
    default:
      EXIT_WITH_ERROR("Got unknown statement to free: %d\n", statement->type);
  }

  free(statement);
}

void C_ParserFreeProgram(C_AstProgram **program) {
  if (!program || !*program) {
    return;
  }

  if ((*program)->function_declarations) {
    for (int i = 0; i < arrlen((*program)->function_declarations); i++) {
      C_AstFreeFunctionDeclaration((*program)->function_declarations[i]);
    }

    arrfree((*program)->function_declarations);
  }

  free(*program);
  *program = NULL;
}

void C_AstFreeVariable(C_AstVariable **variable) {
  if (!variable || !*variable) {
    return;
  }

  StringFree(&(*variable)->name);
  free(*variable);
  *variable = NULL;
}

void C_AstFreeIf(C_AstIf **if_statement) {
  if (!if_statement || !*if_statement) {
    return;
  }

  C_AstFreeExpression(&(*if_statement)->condition);
  C_AstFreeStatement((*if_statement)->block);
  (*if_statement)->block = NULL;

  if ((*if_statement)->has_else) {
    C_AstFreeStatement((*if_statement)->else_block);
    (*if_statement)->else_block = NULL;
  }

  free(*if_statement);
  *if_statement = NULL;
}
