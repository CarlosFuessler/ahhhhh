#include "parser.h"

#include "buffer/buffer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *dup_cstr(const char *src) {
  if (src == NULL) {
    return NULL;
  }
  size_t len = strlen(src);
  char *out = malloc(len + 1);
  if (out == NULL) {
    return NULL;
  }
  memcpy(out, src, len + 1);
  return out;
}

int token_has_string(TokenType type) {
  return type == TOKEN_STRING_LITERAL || type == TOKEN_IDENTIFIER ||
         type == TOKEN_KEYWORD;
}

void set_static_error(Token *errtok, int line, int col,
                             const char *message) {
  if (errtok == NULL) {
    return;
  }
  *errtok = (Token){
      .type = TOKEN_ERROR,
      .line = line,
      .col = col,
      .value.string = (char *)message,
  };
}

void set_error_token(Token *errtok, Token tok) {
  if (errtok == NULL) {
    return;
  }
  *errtok = tok;
  if (token_has_string(tok.type) && tok.value.string != NULL) {
    errtok->value.string = dup_cstr(tok.value.string);
    if (errtok->value.string == NULL) {
      set_static_error(errtok, tok.line, tok.col, "out of memory");
    }
  }
}

int keyword_is(Token tok, const char *word) {
  return tok.type == TOKEN_KEYWORD && tok.value.string != NULL &&
         strcmp(tok.value.string, word) == 0;
}

void skip_newlines(TokenStream *tokens) {
  while (token_stream_peek(tokens, 0).type == TOKEN_NEWLINE) {
    token_stream_read(tokens);
  }
}

int append_expression(AstExpression **items, size_t *len, size_t *cap,
                             AstExpression item) {
  if (*len >= *cap) {
    size_t next_cap = *cap == 0 ? 4 : (*cap * 2);
    AstExpression *next = realloc(*items, next_cap * sizeof(*next));
    if (next == NULL) {
      return -1;
    }
    *items = next;
    *cap = next_cap;
  }
  (*items)[*len] = item;
  *len += 1;
  return 0;
}

int append_statement(AstStatement **items, size_t *len, size_t *cap,
                            AstStatement item) {
  if (*len >= *cap) {
    size_t next_cap = *cap == 0 ? 4 : (*cap * 2);
    AstStatement *next = realloc(*items, next_cap * sizeof(*next));
    if (next == NULL) {
      return -1;
    }
    *items = next;
    *cap = next_cap;
  }
  (*items)[*len] = item;
  *len += 1;
  return 0;
}

int append_string(char ***items, size_t *len, size_t *cap, char *item) {
  if (*len >= *cap) {
    size_t next_cap = *cap == 0 ? 4 : (*cap * 2);
    char **next = realloc(*items, next_cap * sizeof(*next));
    if (next == NULL) {
      return -1;
    }
    *items = next;
    *cap = next_cap;
  }
  (*items)[*len] = item;
  *len += 1;
  return 0;
}

int append_switch_clause(AstSwitchClause **items, size_t *len, size_t *cap,
                                AstSwitchClause item) {
  if (*len >= *cap) {
    size_t next_cap = *cap == 0 ? 4 : (*cap * 2);
    AstSwitchClause *next = realloc(*items, next_cap * sizeof(*next));
    if (next == NULL) {
      return -1;
    }
    *items = next;
    *cap = next_cap;
  }
  (*items)[*len] = item;
  *len += 1;
  return 0;
}

static void print_indent(int depth, FILE *out) {
  for (int i = 0; i < depth; i++) {
    fputs("  ", out);
  }
}

const char *token_symbol(TokenType type) {
  switch (type) {
  case TOKEN_OPEN_PAREN:
    return "(";
  case TOKEN_CLOSE_PAREN:
    return ")";
  case TOKEN_OPEN_BRACE:
    return "{";
  case TOKEN_CLOSE_BRACE:
    return "}";
  case TOKEN_OPEN_BRACKET:
    return "[";
  case TOKEN_CLOSE_BRACKET:
    return "]";
  case TOKEN_COMMA:
    return ",";
  case TOKEN_COLON:
    return ":";
  case TOKEN_EQUALS:
    return "=";
  case TOKEN_DOUBLE_EQUALS:
    return "==";
  case TOKEN_NOT_EQUALS:
    return "!=";
  case TOKEN_LESS:
    return "<";
  case TOKEN_LESS_EQUALS:
    return "<=";
  case TOKEN_GREATER:
    return ">";
  case TOKEN_GREATER_EQUALS:
    return ">=";
  case TOKEN_PLUS:
    return "+";
  case TOKEN_MINUS:
    return "-";
  case TOKEN_ASTERISK:
    return "*";
  case TOKEN_SLASH:
    return "/";
  case TOKEN_PERCENT:
    return "%";
  case TOKEN_AMPERSAND:
    return "&";
  case TOKEN_AT:
    return "@";
  case TOKEN_EXCLAMATION_MARK:
    return "!";
  case TOKEN_DOT:
    return ".";
  case TOKEN_DOUBLE_DOT:
    return "..";
  default:
    return NULL;
  }
}

int buffer_push_cstr(CharBuffer *buffer, const char *str) {
  for (size_t i = 0; str[i] != '\0'; i++) {
    if (buffer_push(buffer, str[i]) < 0) {
      return -1;
    }
  }
  return 0;
}

int append_token_text(CharBuffer *buffer, Token tok) {
  if (tok.type == TOKEN_NUMBER_LITERAL) {
    char numbuf[64];
    snprintf(numbuf, sizeof(numbuf), "%.15g", tok.value.number);
    return buffer_push_cstr(buffer, numbuf);
  }

  if (token_has_string(tok.type) && tok.value.string != NULL) {
    return buffer_push_cstr(buffer, tok.value.string);
  }

  const char *symbol = token_symbol(tok.type);
  if (symbol != NULL) {
    return buffer_push_cstr(buffer, symbol);
  }

  return 0;
}

// void ast_lvalue_free(astlvalueexpression *node);
static int ast_block_parse(TokenStream *tokens, AstBlock *node, Token *errtok);
// void ast_block_free(astblock *node);
// void ast_block_print(astblock *node, int depth, file *out);
// void ast_switch_clause_free(astswitchclause *clause);

static int ast_expression_parse_assignment(TokenStream *tokens,
                                           AstExpression *node,
                                           Token *errtok);

int alloc_expression_ptr(TokenStream *tokens, Token *errtok,
                                AstExpression value, AstExpression **out) {
  AstExpression *ptr = malloc(sizeof(*ptr));
  if (ptr == NULL) {
    Token tok = token_stream_peek(tokens, 0);
    set_static_error(errtok, tok.line, tok.col, "out of memory");
    return -1;
  }
  *ptr = value;
  *out = ptr;
  return 0;
}

int alloc_statement_ptr(TokenStream *tokens, Token *errtok,
                               AstStatement value, AstStatement **out) {
  AstStatement *ptr = malloc(sizeof(*ptr));
  if (ptr == NULL) {
    Token tok = token_stream_peek(tokens, 0);
    set_static_error(errtok, tok.line, tok.col, "out of memory");
    return -1;
  }
  *ptr = value;
  *out = ptr;
  return 0;
}

int alloc_block_ptr(TokenStream *tokens, Token *errtok, AstBlock value,
                           AstBlock **out) {
  AstBlock *ptr = malloc(sizeof(*ptr));
  if (ptr == NULL) {
    Token tok = token_stream_peek(tokens, 0);
    set_static_error(errtok, tok.line, tok.col, "out of memory");
    return -1;
  }
  *ptr = value;
  *out = ptr;
  return 0;
}

int expect_type(TokenStream *tokens, TokenType type, Token *out,
                       Token *errtok) {
  Token tok = token_stream_peek(tokens, 0);
  if (tok.type != type) {
    set_error_token(errtok, tok);
    return -1;
  }
  *out = token_stream_read(tokens);
  return 0;
}

int expect_keyword(TokenStream *tokens, const char *word, Token *out,
                          Token *errtok) {
  Token tok = token_stream_peek(tokens, 0);
  if (!keyword_is(tok, word)) {
    set_error_token(errtok, tok);
    return -1;
  }
  *out = token_stream_read(tokens);
  return 0;
}


static int parse_type_text_until(TokenStream *tokens, char **out, Token *errtok,
                                 int (*stop_fn)(TokenType)) {
  CharBuffer buffer;
  buffer_init(&buffer);
  int added = 0;

  while (1) {
    Token tok = token_stream_peek(tokens, 0);
    if (tok.type == TOKEN_EOF || stop_fn(tok.type)) {
      break;
    }
    if (tok.type == TOKEN_NEWLINE) {
      token_stream_read(tokens);
      continue;
    }
    tok = token_stream_read(tokens);
    if (append_token_text(&buffer, tok) < 0) {
      buffer_free(&buffer);
      set_static_error(errtok, tok.line, tok.col, "out of memory");
      return -1;
    }
    added = 1;
  }

  if (!added) {
    buffer_free(&buffer);
    *out = NULL;
    return 0;
  }

  if (buffer_push(&buffer, '\0') < 0) {
    buffer_free(&buffer);
    Token tok = token_stream_peek(tokens, 0);
    set_static_error(errtok, tok.line, tok.col, "out of memory");
    return -1;
  }

  *out = buffer_release(&buffer);
  return 0;
}

static int stop_on_var_decl_type(TokenType type) {
  return type == TOKEN_EQUALS || type == TOKEN_NEWLINE || type == TOKEN_EOF ||
         type == TOKEN_CLOSE_BRACE;
}

static int stop_on_fn_return_type(TokenType type) {
  return type == TOKEN_OPEN_BRACE || type == TOKEN_EOF;
}

static int stop_on_struct_field_type(TokenType type) {
  return type == TOKEN_COMMA || type == TOKEN_NEWLINE ||
         type == TOKEN_CLOSE_BRACE || type == TOKEN_EOF;
}

static int parse_var_decl(TokenStream *tokens, AstStatement *node, Token *errtok,
                          int is_const) {
  token_stream_read(tokens);

  Token name_tok = token_stream_peek(tokens, 0);
  if (name_tok.type != TOKEN_IDENTIFIER) {
    set_error_token(errtok, name_tok);
    return -1;
  }
  name_tok = token_stream_read(tokens);

  char *name = dup_cstr(name_tok.value.string);
  if (name == NULL) {
    set_static_error(errtok, name_tok.line, name_tok.col, "out of memory");
    return -1;
  }

  char *type_name = NULL;
  skip_newlines(tokens);
  if (token_stream_peek(tokens, 0).type == TOKEN_COLON) {
    token_stream_read(tokens);
    if (parse_type_text_until(tokens, &type_name, errtok, stop_on_var_decl_type) <
        0) {
      free(name);
      return -1;
    }
  }

  AstExpression *init_ptr = NULL;
  skip_newlines(tokens);
  if (token_stream_peek(tokens, 0).type == TOKEN_EQUALS) {
    token_stream_read(tokens);
    AstExpression init_expr = {0};
    if (ast_expression_parse(tokens, &init_expr, errtok) < 0) {
      free(type_name);
      free(name);
      return -1;
    }
    if (alloc_expression_ptr(tokens, errtok, init_expr, &init_ptr) < 0) {
      ast_expression_free(&init_expr);
      free(type_name);
      free(name);
      return -1;
    }
  }

  node->kind = AST_STMT_VAR_DECL;
  node->stmt.var_decl.is_const = is_const;
  node->stmt.var_decl.name = name;
  node->stmt.var_decl.type_name = type_name;
  node->stmt.var_decl.initializer = init_ptr;
  return 0;
}

static int parse_return_stmt(TokenStream *tokens, AstStatement *node,
                             Token *errtok) {
  token_stream_read(tokens);
  skip_newlines(tokens);

  Token next = token_stream_peek(tokens, 0);
  node->kind = AST_STMT_RETURN;
  node->stmt.return_stmt.has_value = 0;
  node->stmt.return_stmt.value = NULL;

  if (next.type == TOKEN_NEWLINE || next.type == TOKEN_EOF ||
      next.type == TOKEN_CLOSE_BRACE) {
    return 0;
  }

  AstExpression value = {0};
  if (ast_expression_parse(tokens, &value, errtok) < 0) {
    return -1;
  }

  AstExpression *value_ptr = NULL;
  if (alloc_expression_ptr(tokens, errtok, value, &value_ptr) < 0) {
    ast_expression_free(&value);
    return -1;
  }

  node->stmt.return_stmt.has_value = 1;
  node->stmt.return_stmt.value = value_ptr;
  return 0;
}

static int parse_condition(TokenStream *tokens, AstExpression **out,
                           Token *errtok) {
  skip_newlines(tokens);
  if (token_stream_peek(tokens, 0).type == TOKEN_OPEN_PAREN) {
    token_stream_read(tokens);
    AstExpression inner = {0};
    if (ast_expression_parse(tokens, &inner, errtok) < 0) {
      return -1;
    }
    Token close;
    if (expect_type(tokens, TOKEN_CLOSE_PAREN, &close, errtok) < 0) {
      ast_expression_free(&inner);
      return -1;
    }
    return alloc_expression_ptr(tokens, errtok, inner, out);
  }

  AstExpression expr = {0};
  if (ast_expression_parse(tokens, &expr, errtok) < 0) {
    return -1;
  }
  return alloc_expression_ptr(tokens, errtok, expr, out);
}

static int parse_if_stmt(TokenStream *tokens, AstStatement *node,
                         Token *errtok) {
  token_stream_read(tokens);

  AstExpression *condition = NULL;
  if (parse_condition(tokens, &condition, errtok) < 0) {
    return -1;
  }

  AstBlock then_block;
  if (ast_block_parse(tokens, &then_block, errtok) < 0) {
    ast_expression_free(condition);
    free(condition);
    return -1;
  }

  AstBlock *then_ptr = NULL;
  if (alloc_block_ptr(tokens, errtok, then_block, &then_ptr) < 0) {
    ast_block_free(&then_block);
    ast_expression_free(condition);
    free(condition);
    return -1;
  }

  AstStatement *else_branch = NULL;
  skip_newlines(tokens);
  Token next = token_stream_peek(tokens, 0);
  if (keyword_is(next, "else")) {
    token_stream_read(tokens);
    skip_newlines(tokens);

    Token follow = token_stream_peek(tokens, 0);
    if (keyword_is(follow, "if")) {
      AstStatement nested_if = {0};
      if (parse_if_stmt(tokens, &nested_if, errtok) < 0) {
        ast_block_free(then_ptr);
        free(then_ptr);
        ast_expression_free(condition);
        free(condition);
        return -1;
      }
      if (alloc_statement_ptr(tokens, errtok, nested_if, &else_branch) < 0) {
        ast_statement_free(&nested_if);
        ast_block_free(then_ptr);
        free(then_ptr);
        ast_expression_free(condition);
        free(condition);
        return -1;
      }
    } else if (follow.type == TOKEN_OPEN_BRACE) {
      AstBlock else_block;
      if (ast_block_parse(tokens, &else_block, errtok) < 0) {
        ast_block_free(then_ptr);
        free(then_ptr);
        ast_expression_free(condition);
        free(condition);
        return -1;
      }
      AstStatement wrapped = {
          .kind = AST_STMT_BLOCK,
          .stmt.block = else_block,
      };
      if (alloc_statement_ptr(tokens, errtok, wrapped, &else_branch) < 0) {
        ast_block_free(&else_block);
        ast_block_free(then_ptr);
        free(then_ptr);
        ast_expression_free(condition);
        free(condition);
        return -1;
      }
    } else {
      set_error_token(errtok, follow);
      ast_block_free(then_ptr);
      free(then_ptr);
      ast_expression_free(condition);
      free(condition);
      return -1;
    }
  }

  node->kind = AST_STMT_IF;
  node->stmt.if_stmt.condition = condition;
  node->stmt.if_stmt.then_block = then_ptr;
  node->stmt.if_stmt.else_branch = else_branch;
  return 0;
}

static int parse_while_stmt(TokenStream *tokens, AstStatement *node,
                            Token *errtok) {
  token_stream_read(tokens);

  AstExpression *condition = NULL;
  if (parse_condition(tokens, &condition, errtok) < 0) {
    return -1;
  }

  AstBlock body;
  if (ast_block_parse(tokens, &body, errtok) < 0) {
    ast_expression_free(condition);
    free(condition);
    return -1;
  }

  AstBlock *body_ptr = NULL;
  if (alloc_block_ptr(tokens, errtok, body, &body_ptr) < 0) {
    ast_block_free(&body);
    ast_expression_free(condition);
    free(condition);
    return -1;
  }

  node->kind = AST_STMT_WHILE;
  node->stmt.while_stmt.condition = condition;
  node->stmt.while_stmt.body = body_ptr;
  return 0;
}

static int parse_for_stmt(TokenStream *tokens, AstStatement *node,
                          Token *errtok) {
  token_stream_read(tokens);

  Token var_name = token_stream_peek(tokens, 0);
  if (var_name.type != TOKEN_IDENTIFIER) {
    set_error_token(errtok, var_name);
    return -1;
  }
  var_name = token_stream_read(tokens);
  char *name = dup_cstr(var_name.value.string);
  if (name == NULL) {
    set_static_error(errtok, var_name.line, var_name.col, "out of memory");
    return -1;
  }

  Token in_kw;
  if (expect_keyword(tokens, "in", &in_kw, errtok) < 0) {
    free(name);
    return -1;
  }

  AstExpression start = {0};
  if (ast_expression_parse(tokens, &start, errtok) < 0) {
    free(name);
    return -1;
  }

  Token range_tok;
  if (expect_type(tokens, TOKEN_DOUBLE_DOT, &range_tok, errtok) < 0) {
    ast_expression_free(&start);
    free(name);
    return -1;
  }

  AstExpression end = {0};
  if (ast_expression_parse(tokens, &end, errtok) < 0) {
    ast_expression_free(&start);
    free(name);
    return -1;
  }

  AstBlock body;
  if (ast_block_parse(tokens, &body, errtok) < 0) {
    ast_expression_free(&start);
    ast_expression_free(&end);
    free(name);
    return -1;
  }

  AstExpression *start_ptr = NULL;
  AstExpression *end_ptr = NULL;
  AstBlock *body_ptr = NULL;

  if (alloc_expression_ptr(tokens, errtok, start, &start_ptr) < 0) {
    ast_expression_free(&start);
    ast_expression_free(&end);
    ast_block_free(&body);
    free(name);
    return -1;
  }
  if (alloc_expression_ptr(tokens, errtok, end, &end_ptr) < 0) {
    ast_expression_free(start_ptr);
    free(start_ptr);
    ast_expression_free(&end);
    ast_block_free(&body);
    free(name);
    return -1;
  }
  if (alloc_block_ptr(tokens, errtok, body, &body_ptr) < 0) {
    ast_expression_free(start_ptr);
    free(start_ptr);
    ast_expression_free(end_ptr);
    free(end_ptr);
    ast_block_free(&body);
    free(name);
    return -1;
  }

  node->kind = AST_STMT_FOR_RANGE;
  node->stmt.for_stmt.name = name;
  node->stmt.for_stmt.start = start_ptr;
  node->stmt.for_stmt.end = end_ptr;
  node->stmt.for_stmt.body = body_ptr;
  return 0;
}

static int parse_switch_stmt(TokenStream *tokens, AstStatement *node,
                             Token *errtok) {
  token_stream_read(tokens);

  AstExpression subject = {0};
  if (ast_expression_parse(tokens, &subject, errtok) < 0) {
    return -1;
  }
  AstExpression *subject_ptr = NULL;
  if (alloc_expression_ptr(tokens, errtok, subject, &subject_ptr) < 0) {
    ast_expression_free(&subject);
    return -1;
  }

  Token open;
  if (expect_type(tokens, TOKEN_OPEN_BRACE, &open, errtok) < 0) {
    ast_expression_free(subject_ptr);
    free(subject_ptr);
    return -1;
  }

  AstSwitchClause *clauses = NULL;
  size_t clause_count = 0;
  size_t clause_cap = 0;

  while (1) {
    skip_newlines(tokens);
    Token tok = token_stream_peek(tokens, 0);
    if (tok.type == TOKEN_CLOSE_BRACE) {
      token_stream_read(tokens);
      break;
    }
    if (tok.type == TOKEN_EOF) {
      for (size_t i = 0; i < clause_count; i++) {
        ast_switch_clause_free(&clauses[i]);
      }
      free(clauses);
      ast_expression_free(subject_ptr);
      free(subject_ptr);
      set_error_token(errtok, tok);
      return -1;
    }

    AstSwitchClause clause = {
        .is_default = 0,
        .value = NULL,
        .statements = NULL,
        .len = 0,
    };

    if (keyword_is(tok, "case")) {
      token_stream_read(tokens);
      AstExpression val = {0};
      if (ast_expression_parse(tokens, &val, errtok) < 0) {
        for (size_t i = 0; i < clause_count; i++) {
          ast_switch_clause_free(&clauses[i]);
        }
        free(clauses);
        ast_expression_free(subject_ptr);
        free(subject_ptr);
        return -1;
      }
      if (alloc_expression_ptr(tokens, errtok, val, &clause.value) < 0) {
        ast_expression_free(&val);
        for (size_t i = 0; i < clause_count; i++) {
          ast_switch_clause_free(&clauses[i]);
        }
        free(clauses);
        ast_expression_free(subject_ptr);
        free(subject_ptr);
        return -1;
      }
    } else if (keyword_is(tok, "default")) {
      token_stream_read(tokens);
      clause.is_default = 1;
    } else {
      for (size_t i = 0; i < clause_count; i++) {
        ast_switch_clause_free(&clauses[i]);
      }
      free(clauses);
      ast_expression_free(subject_ptr);
      free(subject_ptr);
      set_error_token(errtok, tok);
      return -1;
    }

    Token colon;
    if (expect_type(tokens, TOKEN_COLON, &colon, errtok) < 0) {
      ast_switch_clause_free(&clause);
      for (size_t i = 0; i < clause_count; i++) {
        ast_switch_clause_free(&clauses[i]);
      }
      free(clauses);
      ast_expression_free(subject_ptr);
      free(subject_ptr);
      return -1;
    }

    AstStatement *stmts = NULL;
    size_t len = 0;
    size_t cap = 0;

    while (1) {
      skip_newlines(tokens);
      Token look = token_stream_peek(tokens, 0);
      if (look.type == TOKEN_CLOSE_BRACE || keyword_is(look, "case") ||
          keyword_is(look, "default")) {
        break;
      }
      AstStatement stmt = {0};
      if (ast_statement_parse(tokens, &stmt, errtok) < 0) {
        for (size_t i = 0; i < len; i++) {
          ast_statement_free(&stmts[i]);
        }
        free(stmts);
        ast_switch_clause_free(&clause);
        for (size_t i = 0; i < clause_count; i++) {
          ast_switch_clause_free(&clauses[i]);
        }
        free(clauses);
        ast_expression_free(subject_ptr);
        free(subject_ptr);
        return -1;
      }
      if (append_statement(&stmts, &len, &cap, stmt) < 0) {
        ast_statement_free(&stmt);
        for (size_t i = 0; i < len; i++) {
          ast_statement_free(&stmts[i]);
        }
        free(stmts);
        ast_switch_clause_free(&clause);
        for (size_t i = 0; i < clause_count; i++) {
          ast_switch_clause_free(&clauses[i]);
        }
        free(clauses);
        ast_expression_free(subject_ptr);
        free(subject_ptr);
        set_static_error(errtok, look.line, look.col, "out of memory");
        return -1;
      }
    }

    clause.statements = stmts;
    clause.len = len;

    if (append_switch_clause(&clauses, &clause_count, &clause_cap, clause) < 0) {
      ast_switch_clause_free(&clause);
      for (size_t i = 0; i < clause_count; i++) {
        ast_switch_clause_free(&clauses[i]);
      }
      free(clauses);
      ast_expression_free(subject_ptr);
      free(subject_ptr);
      set_static_error(errtok, tok.line, tok.col, "out of memory");
      return -1;
    }
  }

  node->kind = AST_STMT_SWITCH;
  node->stmt.switch_stmt.subject = subject_ptr;
  node->stmt.switch_stmt.clauses = clauses;
  node->stmt.switch_stmt.clause_count = clause_count;
  return 0;
}

static int parse_annotation_stmt(TokenStream *tokens, AstStatement *node,
                                 Token *errtok) {
  Token at_tok;
  if (expect_type(tokens, TOKEN_AT, &at_tok, errtok) < 0) {
    return -1;
  }

  Token open;
  if (expect_type(tokens, TOKEN_OPEN_PAREN, &open, errtok) < 0) {
    return -1;
  }

  CharBuffer buffer;
  buffer_init(&buffer);
  int depth = 1;
  while (depth > 0) {
    Token tok = token_stream_peek(tokens, 0);
    if (tok.type == TOKEN_EOF) {
      buffer_free(&buffer);
      set_error_token(errtok, tok);
      return -1;
    }
    tok = token_stream_read(tokens);
    if (tok.type == TOKEN_OPEN_PAREN) {
      depth += 1;
    } else if (tok.type == TOKEN_CLOSE_PAREN) {
      depth -= 1;
      if (depth == 0) {
        break;
      }
    }

    if (append_token_text(&buffer, tok) < 0) {
      buffer_free(&buffer);
      set_static_error(errtok, tok.line, tok.col, "out of memory");
      return -1;
    }
  }

  if (buffer_push(&buffer, '\0') < 0) {
    buffer_free(&buffer);
    set_static_error(errtok, at_tok.line, at_tok.col, "out of memory");
    return -1;
  }

  node->kind = AST_STMT_ANNOTATION;
  node->stmt.annotation.value = buffer_release(&buffer);
  node->stmt.annotation.alias = NULL;

  Token next = token_stream_peek(tokens, 0);
  if (next.type == TOKEN_IDENTIFIER && next.value.string && strcmp(next.value.string, "as") == 0) {
    token_stream_read(tokens); // "as" konsumieren
    Token alias_tok;
    if (expect_type(tokens, TOKEN_IDENTIFIER, &alias_tok, errtok) < 0) {
      free(node->stmt.annotation.value);
      node->stmt.annotation.value = NULL;
      return -1;
    }
    node->stmt.annotation.alias = dup_cstr(alias_tok.value.string);
  }
  return 0;
}

static int stop_on_fn_param_type(TokenType type) {
  return type == TOKEN_COMMA || type == TOKEN_CLOSE_PAREN || type == TOKEN_EOF;
}

static int skip_balanced_brace_body(TokenStream *tokens, Token *errtok) {
  Token open;
  if (expect_type(tokens, TOKEN_OPEN_BRACE, &open, errtok) < 0) {
    return -1;
  }

  int depth = 1;
  while (depth > 0) {
    Token tok = token_stream_peek(tokens, 0);
    if (tok.type == TOKEN_EOF) {
      set_error_token(errtok, tok);
      return -1;
    }
    tok = token_stream_read(tokens);
    if (tok.type == TOKEN_OPEN_BRACE) {
      depth += 1;
    } else if (tok.type == TOKEN_CLOSE_BRACE) {
      depth -= 1;
    }
  }
  return 0;
}

static int parse_fn_decl(TokenStream *tokens, AstStatement *node,
                         Token *errtok, int is_export, int fn_already_consumed) {
  if (!fn_already_consumed) {
    Token fn_tok;
    if (expect_keyword(tokens, "fn", &fn_tok, errtok) < 0) {
      return -1;
    }
  }

  Token name_tok = token_stream_peek(tokens, 0);
  if (name_tok.type != TOKEN_IDENTIFIER) {
    set_error_token(errtok, name_tok);
    return -1;
  }
  name_tok = token_stream_read(tokens);
  char *name = NULL;
  if (token_stream_peek(tokens, 0).type == TOKEN_DOT) {
    token_stream_read(tokens); // Punkt konsumieren
    Token method_tok = token_stream_peek(tokens, 0);
    if (method_tok.type != TOKEN_IDENTIFIER) {
      set_error_token(errtok, method_tok);
      return -1;
    }
    method_tok = token_stream_read(tokens);
    size_t len = strlen(name_tok.value.string) + 1 + strlen(method_tok.value.string) + 1;
    name = malloc(len);
    if (name == NULL) {
      set_static_error(errtok, name_tok.line, name_tok.col, "out of memory");
      return -1;
    }
    snprintf(name, len, "%s_%s", name_tok.value.string, method_tok.value.string);
  } else {
    name = dup_cstr(name_tok.value.string);
    if (name == NULL) {
      set_static_error(errtok, name_tok.line, name_tok.col, "out of memory");
      return -1;
    }
  }

  Token open_paren;
  if (expect_type(tokens, TOKEN_OPEN_PAREN, &open_paren, errtok) < 0) {
    free(name);
    return -1;
  }

  char **params = NULL;
  char **param_types = NULL;
  size_t param_count = 0;
  size_t param_cap = 0;

  skip_newlines(tokens);
  if (token_stream_peek(tokens, 0).type != TOKEN_CLOSE_PAREN) {
    while (1) {
      Token param_tok = token_stream_peek(tokens, 0);
      if (param_tok.type != TOKEN_IDENTIFIER &&
          !(param_tok.type == TOKEN_KEYWORD &&
            strcmp(param_tok.value.string, "self") == 0)) {
        set_error_token(errtok, param_tok);
        for (size_t i = 0; i < param_count; i++) {
          free(params[i]);
          if (param_types && param_types[i]) free(param_types[i]);
        }
        free(params);
        free(param_types);
        free(name);
        return -1;
      }
      param_tok = token_stream_read(tokens);

      char *param_name = dup_cstr(param_tok.value.string);
      if (param_name == NULL) {
        set_static_error(errtok, param_tok.line, param_tok.col, "out of memory");
        for (size_t i = 0; i < param_count; i++) {
          free(params[i]);
          if (param_types && param_types[i]) free(param_types[i]);
        }
        free(params);
        free(param_types);
        free(name);
        return -1;
      }

      char *param_type = NULL;
      if (token_stream_peek(tokens, 0).type == TOKEN_COLON) {
        token_stream_read(tokens);
        if (parse_type_text_until(tokens, &param_type, errtok, stop_on_fn_param_type) < 0) {
          free(param_name);
          for (size_t i = 0; i < param_count; i++) {
            free(params[i]);
            if (param_types && param_types[i]) free(param_types[i]);
          }
          free(params);
          free(param_types);
          free(name);
          return -1;
        }
      }

      if (param_count >= param_cap) {
        param_cap = param_cap == 0 ? 8 : param_cap * 2;
        params = realloc(params, sizeof(char *) * param_cap);
        param_types = realloc(param_types, sizeof(char *) * param_cap);
      }
      params[param_count] = param_name;
      param_types[param_count] = param_type;
      param_count++;

      skip_newlines(tokens);
      if (token_stream_peek(tokens, 0).type == TOKEN_COMMA) {
        token_stream_read(tokens);
        skip_newlines(tokens);
        continue;
      }
      break;
    }
  }

  Token close_paren;
  if (expect_type(tokens, TOKEN_CLOSE_PAREN, &close_paren, errtok) < 0) {
    for (size_t i = 0; i < param_count; i++) {
      free(params[i]);
      if (param_types && param_types[i]) free(param_types[i]);
    }
    free(params);
    free(param_types);
    free(name);
    return -1;
  }

  char *return_type = NULL;
  skip_newlines(tokens);
  if (token_stream_peek(tokens, 0).type != TOKEN_OPEN_BRACE) {
    if (parse_type_text_until(tokens, &return_type, errtok, stop_on_fn_return_type) <
        0) {
      for (size_t i = 0; i < param_count; i++) {
        free(params[i]);
        if (param_types && param_types[i]) free(param_types[i]);
      }
      free(params);
      free(param_types);
      free(name);
      return -1;
    }
  }

  AstBlock body;
  if (ast_block_parse(tokens, &body, errtok) < 0) {
    free(return_type);
    for (size_t i = 0; i < param_count; i++) {
      free(params[i]);
      if (param_types && param_types[i]) free(param_types[i]);
    }
    free(params);
    free(param_types);
    free(name);
    return -1;
  }

  AstBlock *body_ptr = NULL;
  if (alloc_block_ptr(tokens, errtok, body, &body_ptr) < 0) {
    ast_block_free(&body);
    free(return_type);
    for (size_t i = 0; i < param_count; i++) {
      free(params[i]);
      if (param_types && param_types[i]) free(param_types[i]);
    }
    free(params);
    free(param_types);
    free(name);
    return -1;
  }

  node->kind = AST_STMT_FN_DECL;
  node->stmt.fn_decl.is_export = is_export;
  node->stmt.fn_decl.name = name;
  node->stmt.fn_decl.params = params;
  node->stmt.fn_decl.param_types = param_types;
  node->stmt.fn_decl.param_count = param_count;
  node->stmt.fn_decl.return_type = return_type;
  node->stmt.fn_decl.body = body_ptr;
  return 0;
}

static int parse_type_decl(TokenStream *tokens, AstStatement *node,
                           Token *errtok, int is_enum) {
  token_stream_read(tokens);

  Token name_tok = token_stream_peek(tokens, 0);
  if (name_tok.type != TOKEN_IDENTIFIER) {
    set_error_token(errtok, name_tok);
    return -1;
  }
  name_tok = token_stream_read(tokens);
  char *name = dup_cstr(name_tok.value.string);
  if (name == NULL) {
    set_static_error(errtok, name_tok.line, name_tok.col, "out of memory");
    return -1;
  }

  char **fields = NULL;
  char **field_types = NULL;
  size_t field_count = 0;
  size_t field_cap = 0;

  if (is_enum) {
    if (skip_balanced_brace_body(tokens, errtok) < 0) {
      free(name);
      return -1;
    }
  } else {
    Token open;
    if (expect_type(tokens, TOKEN_OPEN_BRACE, &open, errtok) < 0) {
      free(name);
      return -1;
    }

    while (1) {
      skip_newlines(tokens);
      Token tok = token_stream_peek(tokens, 0);
      if (tok.type == TOKEN_CLOSE_BRACE) {
        token_stream_read(tokens);
        break;
      }
      if (tok.type == TOKEN_EOF) {
        set_error_token(errtok, tok);
        for (size_t i = 0; i < field_count; i++) {
          free(fields[i]);
        }
        free(fields);
        free(name);
        return -1;
      }

      if (keyword_is(tok, "fn")) {
        AstStatement method = {0};
        if (parse_fn_decl(tokens, &method, errtok, 0, 0) < 0) {
          for (size_t i = 0; i < field_count; i++) {
            free(fields[i]);
          }
          free(fields);
          free(name);
          return -1;
        }
        ast_statement_free(&method);
        skip_newlines(tokens);
        continue;
      }

      if (tok.type != TOKEN_IDENTIFIER) {
        set_error_token(errtok, tok);
        for (size_t i = 0; i < field_count; i++) {
          free(fields[i]);
        }
        free(fields);
        free(name);
        return -1;
      }

      tok = token_stream_read(tokens);
      char *field_name = dup_cstr(tok.value.string);
      if (field_name == NULL) {
        set_static_error(errtok, tok.line, tok.col, "out of memory");
        for (size_t i = 0; i < field_count; i++) {
          free(fields[i]);
          if (field_types && field_types[i]) free(field_types[i]);
        }
        free(fields);
        if (field_types) free(field_types);
        free(name);
        return -1;
      }

      char *field_type = NULL;
      skip_newlines(tokens);
      if (token_stream_peek(tokens, 0).type == TOKEN_COLON) {
          token_stream_read(tokens);
          if (parse_type_text_until(tokens, &field_type, errtok,
                                    stop_on_struct_field_type) < 0) {
            free(field_name);
            for (size_t i = 0; i < field_count; i++) {
              free(fields[i]);
              if (field_types && field_types[i]) free(field_types[i]);
            }
            free(fields);
            if (field_types) free(field_types);
            free(name);
            return -1;
          }
      }

      for (size_t i = 0; i < field_count; i++) {
        if (strcmp(fields[i], field_name) == 0) {
          set_static_error(errtok, tok.line, tok.col, "duplicate struct field");
          free(field_name);
          if (field_type) free(field_type);
          for (size_t j = 0; j < field_count; j++) {
            free(fields[j]);
            if (field_types && field_types[j]) free(field_types[j]);
          }
          free(fields);
          if (field_types) free(field_types);
          free(name);
          return -1;
        }
      }

      if (field_count >= field_cap) {
          field_cap = field_cap == 0 ? 8 : field_cap * 2;
          fields = realloc(fields, sizeof(char *) * field_cap);
          field_types = realloc(field_types, sizeof(char *) * field_cap);
      }
      fields[field_count] = field_name;
      field_types[field_count] = field_type;
      field_count++;

      skip_newlines(tokens);
      Token maybe_comma = token_stream_peek(tokens, 0);
      if (maybe_comma.type == TOKEN_COMMA) {
        token_stream_read(tokens);
      }
    }
  }

  Token maybe_semicolon = token_stream_peek(tokens, 0);
  if (maybe_semicolon.type == TOKEN_IDENTIFIER &&
      maybe_semicolon.value.string != NULL &&
      strcmp(maybe_semicolon.value.string, ";") == 0) {
    token_stream_read(tokens);
  }

  node->kind = AST_STMT_TYPE_DECL;
  node->stmt.type_decl.is_enum = is_enum;
  node->stmt.type_decl.name = name;
  node->stmt.type_decl.fields = fields;
  node->stmt.type_decl.field_types = field_types;
  node->stmt.type_decl.field_count = field_count;
  return 0;
}

static int parse_expr_stmt(TokenStream *tokens, AstStatement *node,
                           Token *errtok) {
  AstExpression expr = {0};
  if (ast_expression_parse(tokens, &expr, errtok) < 0) {
    return -1;
  }
  AstExpression *expr_ptr = NULL;
  if (alloc_expression_ptr(tokens, errtok, expr, &expr_ptr) < 0) {
    ast_expression_free(&expr);
    return -1;
  }
  node->kind = AST_STMT_EXPR;
  node->stmt.expr_stmt.expr = expr_ptr;
  return 0;
}

static int ast_block_parse(TokenStream *tokens, AstBlock *node, Token *errtok) {
  Token open;
  if (expect_type(tokens, TOKEN_OPEN_BRACE, &open, errtok) < 0) {
    return -1;
  }

  AstStatement *items = NULL;
  size_t len = 0;
  size_t cap = 0;

  while (1) {
    skip_newlines(tokens);
    Token tok = token_stream_peek(tokens, 0);
    if (tok.type == TOKEN_CLOSE_BRACE) {
      token_stream_read(tokens);
      break;
    }
    if (tok.type == TOKEN_EOF) {
      for (size_t i = 0; i < len; i++) {
        ast_statement_free(&items[i]);
      }
      free(items);
      set_error_token(errtok, tok);
      return -1;
    }

    AstStatement stmt = {0};
    if (ast_statement_parse(tokens, &stmt, errtok) < 0) {
      for (size_t i = 0; i < len; i++) {
        ast_statement_free(&items[i]);
      }
      free(items);
      return -1;
    }

    if (append_statement(&items, &len, &cap, stmt) < 0) {
      ast_statement_free(&stmt);
      for (size_t i = 0; i < len; i++) {
        ast_statement_free(&items[i]);
      }
      free(items);
      set_static_error(errtok, tok.line, tok.col, "out of memory");
      return -1;
    }
  }

  node->statements = items;
  node->len = len;
  return 0;
}

int ast_statement_parse(TokenStream *tokens, AstStatement *node, Token *errtok) {
  skip_newlines(tokens);
  Token tok = token_stream_peek(tokens, 0);

  if (tok.type == TOKEN_AT) {
    return parse_annotation_stmt(tokens, node, errtok);
  }

  if (tok.type == TOKEN_OPEN_BRACE) {
    AstBlock block;
    if (ast_block_parse(tokens, &block, errtok) < 0) {
      return -1;
    }
    node->kind = AST_STMT_BLOCK;
    node->stmt.block = block;
    return 0;
  }

  if (tok.type == TOKEN_KEYWORD) {
    if (keyword_is(tok, "var")) {
      return parse_var_decl(tokens, node, errtok, 0);
    }
    if (keyword_is(tok, "const")) {
      return parse_var_decl(tokens, node, errtok, 1);
    }
    if (keyword_is(tok, "return")) {
      return parse_return_stmt(tokens, node, errtok);
    }
    if (keyword_is(tok, "if")) {
      return parse_if_stmt(tokens, node, errtok);
    }
    if (keyword_is(tok, "while")) {
      return parse_while_stmt(tokens, node, errtok);
    }
    if (keyword_is(tok, "for")) {
      return parse_for_stmt(tokens, node, errtok);
    }
    if (keyword_is(tok, "switch")) {
      return parse_switch_stmt(tokens, node, errtok);
    }
    if (keyword_is(tok, "br")) {
      token_stream_read(tokens);
      node->kind = AST_STMT_BREAK;
      return 0;
    }
    if (keyword_is(tok, "fw")) {
      token_stream_read(tokens);
      node->kind = AST_STMT_CONTINUE;
      return 0;
    }
    if (keyword_is(tok, "fn")) {
      return parse_fn_decl(tokens, node, errtok, 0, 0);
    }
    if (keyword_is(tok, "export")) {
      token_stream_read(tokens);
      Token next = token_stream_peek(tokens, 0);
      if (!keyword_is(next, "fn")) {
        set_error_token(errtok, next);
        return -1;
      }
      token_stream_read(tokens);
      return parse_fn_decl(tokens, node, errtok, 1, 1);
    }
    if (keyword_is(tok, "enum")) {
      return parse_type_decl(tokens, node, errtok, 1);
    }
    if (keyword_is(tok, "struct")) {
      return parse_type_decl(tokens, node, errtok, 0);
    }
  }

  return parse_expr_stmt(tokens, node, errtok);
}

int ast_program_parse(TokenStream *tokens, AstProgram *node, Token *errtok) {
  AstStatement *items = NULL;
  size_t len = 0;
  size_t cap = 0;

  skip_newlines(tokens);
  while (1) {
    Token tok = token_stream_peek(tokens, 0);
    if (tok.type == TOKEN_EOF) {
      break;
    }

    AstStatement stmt = {0};
    if (ast_statement_parse(tokens, &stmt, errtok) < 0) {
      for (size_t i = 0; i < len; i++) {
        ast_statement_free(&items[i]);
      }
      free(items);
      return -1;
    }
    if (append_statement(&items, &len, &cap, stmt) < 0) {
      ast_statement_free(&stmt);
      for (size_t i = 0; i < len; i++) {
        ast_statement_free(&items[i]);
      }
      free(items);
      set_static_error(errtok, tok.line, tok.col, "out of memory");
      return -1;
    }

    skip_newlines(tokens);
  }

  node->statements = items;
  node->len = len;
  return 0;
}
