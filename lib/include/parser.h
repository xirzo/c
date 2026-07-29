#ifndef PARSER_H
#define PARSER_H

#include "error.h"
#include "lexer.h"
#include "xi_string.h"

typedef struct C_AstExpression C_AstExpression;
typedef struct C_AstStatement  C_AstStatement;

typedef enum {
  C_AST_CONSTANT_INT,
  C_AST_CONSTANT_CHAR,
  C_AST_CONSTANT_STRING,
} C_AstConstantType;

typedef enum {
  C_CONSTANT,
  C_FUNCTION_CALL,
  C_VARIABLE,
  C_BINARY_EXPRESSION,
  C_UNARY_EXPRESSION,
} C_AstExpressionType;

typedef enum {
  C_UNARY_DEREF,
  C_UNARY_ADDRESS_OF,
} C_AstUnaryExpressionType;

typedef struct {
  C_AstConstantType type;
  union {
    int    int_value;
    String string_value;
  } value;
} C_AstConstant;

typedef struct {
  String function_name;
} C_AstFunctionCall;

typedef struct {
  String name;
} C_AstVariable;

typedef struct {
  char             symbol;
  C_AstExpression *lhs;
  C_AstExpression *rhs;
} C_AstBinaryExpression;

typedef struct {
  C_AstUnaryExpressionType type;
  C_AstExpression         *operand;
} C_AstUnaryExpression;

typedef struct C_AstExpression {
  C_AstExpressionType type;

  union {
    C_AstConstant         *constant;
    C_AstFunctionCall     *function_call;
    C_AstVariable         *variable;
    C_AstBinaryExpression *binary;
    C_AstUnaryExpression  *unary;
  };
} C_AstExpression;

typedef enum {
  C_STATEMENT_BLOCK,
  C_STATEMENT_RETURN,
  C_STATEMENT_FUNCTION_DECLARATION,
  C_STATEMENT_EXPRESSION,
  C_STATEMENT_ASSIGNMENT,
  C_STATEMENT_IF,
  C_STATEMENT_NOOP,
} C_AstStatementType;

typedef struct {
  C_AstStatement **statements;
} C_AstBlock;

typedef struct {
  C_AstExpression *value;
} C_AstReturn;

typedef struct {
  String      function_name;
  C_AstBlock *body;
} C_AstFunctionDeclaration;

typedef struct {
  String           variable_name;
  C_AstExpression *expression;
  int              pointer_depth;
} C_AstVariableAssignment;

typedef struct {
  C_AstExpression *condition;
  C_AstStatement  *block;
} C_AstIf;

typedef struct C_AstStatement {
  C_AstStatementType type;

  union {
    C_AstBlock               *block;
    C_AstReturn              *return_statement;
    C_AstFunctionDeclaration *function_declaration;
    C_AstExpression          *expression;
    C_AstVariableAssignment  *assignment;
    C_AstIf                  *if_statement;
  };
} C_AstStatement;

typedef struct {
  C_AstFunctionDeclaration **function_declarations;
} C_AstProgram;

typedef struct {
  double left;
  double right;
} C_InfixBindingPower;

typedef struct {
  C_ErrorContext *error_context;
  const char     *filename;
  C_Token        *tokens;
  C_Token         current_token;
  size_t          current_position;
  size_t          read_position;
} C_Parser;

C_Parser     *C_ParserCreate(C_Token *tokens, C_ErrorContext *error_context,
                             const char *filename);
C_AstProgram *C_ParserParse(C_Parser *parser);
void          C_ParserFree(C_Parser **parser);
void          C_ParserFreeProgram(C_AstProgram **program);

void    C_ParserAdvance(C_Parser *parser);
C_Token C_ParserPeek(C_Parser *parser);

C_AstStatement          *C_ParserParseStatement(C_Parser *parser);
C_AstVariableAssignment *C_ParserParseVariableAssignment(C_Parser *parser);
C_InfixBindingPower      C_GetInfixBindingPower(C_TokenType token_type);
C_AstExpression         *C_ParserParseExpression(C_Parser *parser);
C_AstExpression         *C_ParserParseExpressionWithPrecedence(
    C_Parser *parser, double min_binding_power);
C_AstConstant            *C_ParserParseConstant(C_Parser *parser);
C_AstFunctionCall        *C_ParserParseFunctionCall(C_Parser *parser);
C_AstReturn              *C_ParserParseReturn(C_Parser *parser);
C_AstBlock               *C_ParserParseBlock(C_Parser *parser);
C_AstFunctionDeclaration *C_ParserParseFunctionDeclaration(C_Parser *parser);
C_AstVariable            *C_ParserParseVariable(C_Parser *parser);
C_AstIf                  *C_ParserParseIf(C_Parser *parser);

void C_AstFreeExpression(C_AstExpression **expression);
void C_AstFreeStatement(C_AstStatement *statement);
void C_AstFreeBlock(C_AstBlock *block);
void C_AstFreeReturn(C_AstReturn *ret);
void C_AstFreeVariableAssignment(C_AstVariableAssignment *assignment);
void C_AstFreeFunctionDeclaration(C_AstFunctionDeclaration *declaration);
void C_AstFreeVariable(C_AstVariable **variable);
void C_AstFreeIf(C_AstIf **if_statement);

void C_ParserSynchronizeToDeclaration(C_Parser *parser);
void C_ParserSynchronize(C_Parser *parser);

#endif  // !PARSER_H
