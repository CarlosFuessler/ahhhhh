#ifndef PARSER_H
#define PARSER_H

#include "buffer/buffer.h"
#include "lexer/token.h"
#include "lexer/token_stream.h"

#include "stddef.h"
#include "stdio.h"

typedef struct AstExpression AstExpression;
typedef struct AstStatement AstStatement;
typedef struct AstBlock AstBlock;
typedef struct AstProgram AstProgram;
typedef struct AstLvalueExpression AstLvalueExpression;
typedef struct AstSwitchClause AstSwitchClause;

typedef struct {
  double val;
} AstNumberLiteral;

typedef struct {
  char *val;
} AstStringLiteral;

typedef struct {
  int val;
} AstBoolLiteral;

typedef struct {
  enum {
    AST_LITERAL_NUMBER,
    AST_LITERAL_STRING,
    AST_LITERAL_BOOL,
    AST_LITERAL_NULL,
  } kind;
  union {
    AstNumberLiteral number;
    AstStringLiteral string;
    AstBoolLiteral boolean;
  } expr;
} AstLiteralExpression;

struct AstLvalueExpression {
  enum {
    AST_LVALUE_IDENTIFIER,
    AST_LVALUE_BRACKETS,
    AST_LVALUE_DOT,
  } kind;
  union {
    char *identifier;
    struct {
      AstExpression *base;
      AstExpression *index;
    } brackets;
    struct {
      AstExpression *base;
      char *identifier;
    } dot;
  } expr;
};

typedef struct {
  AstLvalueExpression *left;
  AstExpression *right;
} AstAssignmentExpression;

typedef struct {
  AstExpression *function;
  AstExpression *args;
  char **arg_names;
  size_t arg_count;
} AstFunctionCallExpression;

typedef struct {
  AstExpression *expr;
} AstGroupExpression;

typedef struct {
  TokenType op;
  char *keyword;
  AstExpression *operand;
} AstUnaryExpression;

typedef struct {
  TokenType op;
  AstExpression *left;
  AstExpression *right;
} AstBinaryExpression;

typedef struct {
  TokenType op;
  AstLvalueExpression *left;
  AstExpression *right;
} AstCompoundAssignExpression;

typedef struct {
  AstExpression *elements;
  size_t element_count;
} AstArrayLiteralExpression;

struct AstExpression {
  enum {
    AST_EXPR_LITERAL,
    AST_EXPR_LVALUE,
    AST_EXPR_ASSIGNMENT,
    AST_EXPR_COMPOUND_ASSIGN,
    AST_EXPR_FUNCTION_CALL,
    AST_EXPR_GROUP,
    AST_EXPR_UNARY,
    AST_EXPR_BINARY,
    AST_EXPR_ARRAY_LITERAL,
  } kind;
  union {
    AstLiteralExpression literal;
    AstLvalueExpression lvalue;
    AstAssignmentExpression assignment;
    AstCompoundAssignExpression compound_assignment;
    AstFunctionCallExpression function_call;
    AstGroupExpression group;
    AstUnaryExpression unary;
    AstBinaryExpression binary;
    AstArrayLiteralExpression array_literal;
  } expr;
};

typedef struct {
  int is_const;
  char *name;
  char *type_name;
  AstExpression *initializer;
} AstVarDeclStatement;

typedef struct {
  int has_value;
  AstExpression *value;
} AstReturnStatement;

typedef struct {
  AstExpression *condition;
  AstBlock *then_block;
  AstStatement *else_branch;
} AstIfStatement;

typedef struct {
  AstExpression *condition;
  AstBlock *body;
} AstWhileStatement;

typedef struct {
  char *name;
  AstExpression *start;
  AstExpression *end;
  AstBlock *body;
} AstForRangeStatement;

struct AstSwitchClause {
  int is_default;
  AstExpression *value;
  AstStatement *statements;
  size_t len;
};

typedef struct {
  AstExpression *subject;
  AstSwitchClause *clauses;
  size_t clause_count;
} AstSwitchStatement;

typedef struct {
  int is_export;
  char *name;
  char **params;
  char **param_types;
  size_t param_count;
  char *return_type;
  AstBlock *body;
} AstFnDeclStatement;

typedef struct {
  int is_enum;
  char *name;
  char **fields;
  char **field_types;
  size_t field_count;
} AstTypeDeclStatement;

typedef struct {
  AstExpression *expr;
} AstExprStatement;

typedef struct {
  char *value;
  char *alias;
} AstAnnotationStatement;

struct AstBlock {
  AstStatement *statements;
  size_t len;
};

struct AstStatement {
  enum {
    AST_STMT_VAR_DECL,
    AST_STMT_RETURN,
    AST_STMT_IF,
    AST_STMT_WHILE,
    AST_STMT_FOR_RANGE,
    AST_STMT_SWITCH,
    AST_STMT_BREAK,
    AST_STMT_CONTINUE,
    AST_STMT_EXPR,
    AST_STMT_BLOCK,
    AST_STMT_FN_DECL,
    AST_STMT_TYPE_DECL,
    AST_STMT_ANNOTATION,
  } kind;
  union {
    AstVarDeclStatement var_decl;
    AstReturnStatement return_stmt;
    AstIfStatement if_stmt;
    AstWhileStatement while_stmt;
    AstForRangeStatement for_stmt;
    AstSwitchStatement switch_stmt;
    AstExprStatement expr_stmt;
    AstBlock block;
    AstFnDeclStatement fn_decl;
    AstTypeDeclStatement type_decl;
    AstAnnotationStatement annotation;
  } stmt;
};

struct AstProgram {
  AstStatement *statements;
  size_t len;
};

int ast_program_parse(TokenStream *tokens, AstProgram *node, Token *errtok);
void ast_program_free(AstProgram *node);
void ast_program_print(AstProgram *node, FILE *out);

int ast_statement_parse(TokenStream *tokens, AstStatement *node, Token *errtok);
void ast_statement_free(AstStatement *node);
void ast_statement_print(AstStatement *node, int depth, FILE *out);

int ast_expression_parse(TokenStream *tokens, AstExpression *node,
                         Token *errtok);
void ast_expression_free(AstExpression *node);
void ast_expression_print(AstExpression *node, int depth, FILE *out);

// logging and freeing lvalue expressions and blocks is a bit more complicated
// because they can be recursive
void ast_lvalue_free(AstLvalueExpression *node);
void ast_block_free(AstBlock *node);
void ast_switch_clause_free(AstSwitchClause *clause);
void ast_lvalue_print(AstLvalueExpression *node, int depth, FILE *out);
void ast_block_print(AstBlock *node, int depth, FILE *out);

// common helper functions
char *dup_cstr(const char *src);
void set_static_error(Token *errtok, int line, int col, const char *message);
void set_error_token(Token *errtok, Token tok);
int expect_type(TokenStream *tokens, TokenType type, Token *out, Token *errtok);
int expect_keyword(TokenStream *tokens, const char *word, Token *out,
                   Token *errtok);
void skip_newlines(TokenStream *tokens);
int alloc_expression_ptr(TokenStream *tokens, Token *errtok,
                         AstExpression value, AstExpression **out);
int alloc_statement_ptr(TokenStream *tokens, Token *errtok, AstStatement value,
                        AstStatement **out);
int alloc_block_ptr(TokenStream *tokens, Token *errtok, AstBlock value,
                    AstBlock **out);

int token_has_string(TokenType type);
int keyword_is(Token tok, const char *word);
int append_expression(AstExpression **items, size_t *len, size_t *cap,
                      AstExpression item);
int append_statement(AstStatement **items, size_t *len, size_t *cap,
                     AstStatement item);
int append_string(char ***items, size_t *len, size_t *cap, char *item);
int append_switch_clause(AstSwitchClause **items, size_t *len, size_t *cap,
                         AstSwitchClause item);
const char *token_symbol(TokenType type);
int buffer_push_cstr(CharBuffer *buffer, const char *str);
int append_token_text(CharBuffer *buffer, Token tok);

#endif // PARSER_H
