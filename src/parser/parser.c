#include "parser.h"

#include "buffer/buffer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_cstr(const char *src) {
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

static int token_has_string(TokenType type) {
  return type == TOKEN_STRING_LITERAL || type == TOKEN_IDENTIFIER ||
         type == TOKEN_KEYWORD;
}

static void set_static_error(Token *errtok, int line, int col,
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

static void set_error_token(Token *errtok, Token tok) {
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

static int keyword_is(Token tok, const char *word) {
  return tok.type == TOKEN_KEYWORD && tok.value.string != NULL &&
         strcmp(tok.value.string, word) == 0;
}

static void skip_newlines(TokenStream *tokens) {
  while (token_stream_peek(tokens, 0).type == TOKEN_NEWLINE) {
    token_stream_read(tokens);
  }
}

static int append_expression(AstExpression **items, size_t *len, size_t *cap,
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

static int append_statement(AstStatement **items, size_t *len, size_t *cap,
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

static int append_string(char ***items, size_t *len, size_t *cap, char *item) {
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

static int append_switch_clause(AstSwitchClause **items, size_t *len, size_t *cap,
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

static const char *token_symbol(TokenType type) {
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

static int buffer_push_cstr(CharBuffer *buffer, const char *str) {
  for (size_t i = 0; str[i] != '\0'; i++) {
    if (buffer_push(buffer, str[i]) < 0) {
      return -1;
    }
  }
  return 0;
}

static int append_token_text(CharBuffer *buffer, Token tok) {
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

static void ast_lvalue_free(AstLvalueExpression *node);
static int ast_block_parse(TokenStream *tokens, AstBlock *node, Token *errtok);
static void ast_block_free(AstBlock *node);
static void ast_block_print(AstBlock *node, int depth, FILE *out);
static void ast_switch_clause_free(AstSwitchClause *clause);

static int ast_expression_parse_assignment(TokenStream *tokens,
                                           AstExpression *node,
                                           Token *errtok);

static int alloc_expression_ptr(TokenStream *tokens, Token *errtok,
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

static int alloc_statement_ptr(TokenStream *tokens, Token *errtok,
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

static int alloc_block_ptr(TokenStream *tokens, Token *errtok, AstBlock value,
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

static int expect_type(TokenStream *tokens, TokenType type, Token *out,
                       Token *errtok) {
  Token tok = token_stream_peek(tokens, 0);
  if (tok.type != type) {
    set_error_token(errtok, tok);
    return -1;
  }
  *out = token_stream_read(tokens);
  return 0;
}

static int expect_keyword(TokenStream *tokens, const char *word, Token *out,
                          Token *errtok) {
  Token tok = token_stream_peek(tokens, 0);
  if (!keyword_is(tok, word)) {
    set_error_token(errtok, tok);
    return -1;
  }
  *out = token_stream_read(tokens);
  return 0;
}

static int expression_allows_brace_constructor(AstExpression *node) {
  if (node->kind != AST_EXPR_LVALUE) {
    return 0;
  }
  if (node->expr.lvalue.kind != AST_LVALUE_IDENTIFIER) {
    return 0;
  }
  const char *name = node->expr.lvalue.expr.identifier;
  if (name == NULL || name[0] == '\0') {
    return 0;
  }
  return name[0] == '[' || isupper((unsigned char)name[0]);
}

static void free_call_arguments(AstExpression *args, size_t arg_count,
                                char **arg_names) {
  for (size_t i = 0; i < arg_count; i++) {
    ast_expression_free(&args[i]);
  }
  free(args);
  if (arg_names != NULL) {
    for (size_t i = 0; i < arg_count; i++) {
      free(arg_names[i]);
    }
  }
  free(arg_names);
}

static int parse_argument_list(TokenStream *tokens, TokenType close_type,
                               int named_fields, AstExpression **out_args,
                               char ***out_arg_names, size_t *out_count,
                               Token *errtok) {
  AstExpression *args = NULL;
  char **arg_names = NULL;
  size_t len = 0;
  size_t cap = 0;
  size_t name_len = 0;
  size_t name_cap = 0;

  skip_newlines(tokens);
  if (token_stream_peek(tokens, 0).type == close_type) {
    token_stream_read(tokens);
    *out_args = NULL;
    *out_arg_names = NULL;
    *out_count = 0;
    return 0;
  }

  while (1) {
    skip_newlines(tokens);
    char *arg_name = NULL;

    if (named_fields) {
      Token first = token_stream_peek(tokens, 0);
      Token second = token_stream_peek(tokens, 1);
      if (first.type == TOKEN_IDENTIFIER && second.type == TOKEN_COLON) {
        arg_name = dup_cstr(first.value.string);
        if (arg_name == NULL) {
          free_call_arguments(args, len, arg_names);
          Token tok = token_stream_peek(tokens, 0);
          set_static_error(errtok, tok.line, tok.col, "out of memory");
          return -1;
        }
        token_stream_read(tokens);
        token_stream_read(tokens);
      }
    }

    AstExpression arg = {0};
    if (ast_expression_parse(tokens, &arg, errtok) < 0) {
      free(arg_name);
      free_call_arguments(args, len, arg_names);
      return -1;
    }

    if (named_fields && arg_name == NULL && arg.kind == AST_EXPR_LVALUE &&
        arg.expr.lvalue.kind == AST_LVALUE_IDENTIFIER) {
      const char *identifier = arg.expr.lvalue.expr.identifier;
      if (identifier != NULL && identifier[0] != '\0' && identifier[0] != '.') {
        arg_name = dup_cstr(identifier);
        if (arg_name == NULL) {
          ast_expression_free(&arg);
          free_call_arguments(args, len, arg_names);
          Token tok = token_stream_peek(tokens, 0);
          set_static_error(errtok, tok.line, tok.col, "out of memory");
          return -1;
        }
      }
    }

    if (append_expression(&args, &len, &cap, arg) < 0) {
      ast_expression_free(&arg);
      free(arg_name);
      free_call_arguments(args, len, arg_names);
      Token tok = token_stream_peek(tokens, 0);
      set_static_error(errtok, tok.line, tok.col, "out of memory");
      return -1;
    }

    if (named_fields) {
      if (append_string(&arg_names, &name_len, &name_cap, arg_name) < 0) {
        len -= 1;
        ast_expression_free(&args[len]);
        free(arg_name);
        free_call_arguments(args, len, arg_names);
        Token tok = token_stream_peek(tokens, 0);
        set_static_error(errtok, tok.line, tok.col, "out of memory");
        return -1;
      }
    }

    skip_newlines(tokens);
    Token next = token_stream_peek(tokens, 0);
    if (next.type == TOKEN_COMMA) {
      token_stream_read(tokens);
      continue;
    }
    if (next.type == close_type) {
      token_stream_read(tokens);
      break;
    }

    set_error_token(errtok, next);
    free_call_arguments(args, len, arg_names);
    return -1;
  }

  *out_args = args;
  *out_arg_names = arg_names;
  *out_count = len;
  return 0;
}

static int parse_primary(TokenStream *tokens, AstExpression *node,
                         Token *errtok) {
  skip_newlines(tokens);
  Token tok = token_stream_peek(tokens, 0);

  if (tok.type == TOKEN_NUMBER_LITERAL) {
    Token consumed = token_stream_read(tokens);
    node->kind = AST_EXPR_LITERAL;
    node->expr.literal.kind = AST_LITERAL_NUMBER;
    node->expr.literal.expr.number.val = consumed.value.number;
    return 0;
  }

  if (tok.type == TOKEN_STRING_LITERAL) {
    Token consumed = token_stream_read(tokens);
    char *dup = dup_cstr(consumed.value.string);
    if (dup == NULL) {
      set_static_error(errtok, consumed.line, consumed.col, "out of memory");
      return -1;
    }
    node->kind = AST_EXPR_LITERAL;
    node->expr.literal.kind = AST_LITERAL_STRING;
    node->expr.literal.expr.string.val = dup;
    return 0;
  }

  if (tok.type == TOKEN_KEYWORD) {
    if (keyword_is(tok, "true") || keyword_is(tok, "false")) {
      Token consumed = token_stream_read(tokens);
      node->kind = AST_EXPR_LITERAL;
      node->expr.literal.kind = AST_LITERAL_BOOL;
      node->expr.literal.expr.boolean.val =
          strcmp(consumed.value.string, "true") == 0;
      return 0;
    }
    if (keyword_is(tok, "null")) {
      token_stream_read(tokens);
      node->kind = AST_EXPR_LITERAL;
      node->expr.literal.kind = AST_LITERAL_NULL;
      return 0;
    }
    if (keyword_is(tok, "self")) {
      Token consumed = token_stream_read(tokens);
      char *name = dup_cstr(consumed.value.string);
      if (name == NULL) {
        set_static_error(errtok, consumed.line, consumed.col, "out of memory");
        return -1;
      }
      node->kind = AST_EXPR_LVALUE;
      node->expr.lvalue.kind = AST_LVALUE_IDENTIFIER;
      node->expr.lvalue.expr.identifier = name;
      return 0;
    }
  }

  if (tok.type == TOKEN_IDENTIFIER) {
    Token consumed = token_stream_read(tokens);
    char *name = dup_cstr(consumed.value.string);
    if (name == NULL) {
      set_static_error(errtok, consumed.line, consumed.col, "out of memory");
      return -1;
    }
    node->kind = AST_EXPR_LVALUE;
    node->expr.lvalue.kind = AST_LVALUE_IDENTIFIER;
    node->expr.lvalue.expr.identifier = name;
    return 0;
  }

  if (tok.type == TOKEN_DOT) {
    Token dot = token_stream_read(tokens);
    Token member = token_stream_peek(tokens, 0);
    if (member.type != TOKEN_IDENTIFIER && member.type != TOKEN_KEYWORD) {
      set_error_token(errtok, member);
      return -1;
    }
    Token consumed_member = token_stream_read(tokens);
    size_t len = strlen(consumed_member.value.string);
    char *name = malloc(len + 2);
    if (name == NULL) {
      set_static_error(errtok, dot.line, dot.col, "out of memory");
      return -1;
    }
    name[0] = '.';
    memcpy(name + 1, consumed_member.value.string, len + 1);
    node->kind = AST_EXPR_LVALUE;
    node->expr.lvalue.kind = AST_LVALUE_IDENTIFIER;
    node->expr.lvalue.expr.identifier = name;
    return 0;
  }

  if (tok.type == TOKEN_OPEN_PAREN) {
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

    AstExpression *inner_ptr = NULL;
    if (alloc_expression_ptr(tokens, errtok, inner, &inner_ptr) < 0) {
      ast_expression_free(&inner);
      return -1;
    }

    node->kind = AST_EXPR_GROUP;
    node->expr.group.expr = inner_ptr;
    return 0;
  }

  if (tok.type == TOKEN_OPEN_BRACKET) {
    // Scan ahead to decide if this is an array literal or a type identifier hack
    int is_type = 0;
    int depth = 0;
    for (size_t i = 0; ; i++) {
      Token p = token_stream_peek(tokens, i);
      if (p.type == TOKEN_EOF) break;
      if (p.type == TOKEN_OPEN_BRACKET) depth++;
      else if (p.type == TOKEN_CLOSE_BRACKET) {
        depth--;
        if (depth == 0) {
          Token tail = token_stream_peek(tokens, i + 1);
          if (tail.type == TOKEN_IDENTIFIER || tail.type == TOKEN_ASTERISK) {
            is_type = 1;
          }
          break;
        }
      } else if (p.type == TOKEN_COMMA && depth == 1) {
        // Commas at top level of brackets mean it's definitely an array literal
        is_type = 0;
        break;
      }
    }

    if (is_type) {
      CharBuffer buffer;
      buffer_init(&buffer);
      int depth = 0;

      while (1) {
        Token part = token_stream_peek(tokens, 0);
        if (part.type == TOKEN_EOF) {
          buffer_free(&buffer);
          set_error_token(errtok, part);
          return -1;
        }
        part = token_stream_read(tokens);
        if (append_token_text(&buffer, part) < 0) {
          buffer_free(&buffer);
          set_static_error(errtok, part.line, part.col, "out of memory");
          return -1;
        }
        if (part.type == TOKEN_OPEN_BRACKET) {
          depth += 1;
        } else if (part.type == TOKEN_CLOSE_BRACKET) {
          depth -= 1;
          if (depth == 0) {
            while (1) {
              Token tail = token_stream_peek(tokens, 0);
              if (tail.type != TOKEN_IDENTIFIER && tail.type != TOKEN_ASTERISK) {
                break;
              }
              tail = token_stream_read(tokens);
              if (append_token_text(&buffer, tail) < 0) {
                buffer_free(&buffer);
                set_static_error(errtok, tail.line, tail.col, "out of memory");
                return -1;
              }
            }
            break;
          }
        }
      }

      if (buffer_push(&buffer, '\0') < 0) {
        buffer_free(&buffer);
        Token at = token_stream_peek(tokens, 0);
        set_static_error(errtok, at.line, at.col, "out of memory");
        return -1;
      }

      node->kind = AST_EXPR_LVALUE;
      node->expr.lvalue.kind = AST_LVALUE_IDENTIFIER;
      node->expr.lvalue.expr.identifier = buffer_release(&buffer);
      return 0;
    } else {
      token_stream_read(tokens); // consume [
      AstExpression *elements = NULL;
      char **names = NULL;
      size_t count = 0;
      if (parse_argument_list(tokens, TOKEN_CLOSE_BRACKET, 0, &elements, &names,
                              &count, errtok) < 0) {
        return -1;
      }
      node->kind = AST_EXPR_ARRAY_LITERAL;
      node->expr.array_literal.elements = elements;
      node->expr.array_literal.element_count = count;
      return 0;
    }
  }

  set_error_token(errtok, tok);
  return -1;
}

static int parse_postfix(TokenStream *tokens, AstExpression *node,
                         Token *errtok) {
  while (1) {
    Token tok = token_stream_peek(tokens, 0);

    if (tok.type == TOKEN_OPEN_PAREN) {
      token_stream_read(tokens);
      AstExpression *args = NULL;
      char **arg_names = NULL;
      size_t arg_count = 0;
      if (parse_argument_list(tokens, TOKEN_CLOSE_PAREN, 0, &args, &arg_names,
                              &arg_count, errtok) < 0) {
        return -1;
      }

      AstExpression *func = NULL;
      if (alloc_expression_ptr(tokens, errtok, *node, &func) < 0) {
        free_call_arguments(args, arg_count, arg_names);
        return -1;
      }

      node->kind = AST_EXPR_FUNCTION_CALL;
      node->expr.function_call.function = func;
      node->expr.function_call.args = args;
      node->expr.function_call.arg_names = arg_names;
      node->expr.function_call.arg_count = arg_count;
      continue;
    }

    if (tok.type == TOKEN_OPEN_BRACE && expression_allows_brace_constructor(node)) {
      token_stream_read(tokens);
      AstExpression *args = NULL;
      char **arg_names = NULL;
      size_t arg_count = 0;
      if (parse_argument_list(tokens, TOKEN_CLOSE_BRACE, 1, &args, &arg_names,
                              &arg_count, errtok) < 0) {
        return -1;
      }

      AstExpression *func = NULL;
      if (alloc_expression_ptr(tokens, errtok, *node, &func) < 0) {
        free_call_arguments(args, arg_count, arg_names);
        return -1;
      }

      node->kind = AST_EXPR_FUNCTION_CALL;
      node->expr.function_call.function = func;
      node->expr.function_call.args = args;
      node->expr.function_call.arg_names = arg_names;
      node->expr.function_call.arg_count = arg_count;
      continue;
    }

    if (tok.type == TOKEN_OPEN_BRACKET) {
      token_stream_read(tokens);

      AstExpression index_expr = {0};
      if (ast_expression_parse(tokens, &index_expr, errtok) < 0) {
        return -1;
      }

      Token close;
      if (expect_type(tokens, TOKEN_CLOSE_BRACKET, &close, errtok) < 0) {
        ast_expression_free(&index_expr);
        return -1;
      }

      AstExpression *base_ptr = NULL;
      AstExpression *index_ptr = NULL;
      if (alloc_expression_ptr(tokens, errtok, *node, &base_ptr) < 0) {
        ast_expression_free(&index_expr);
        return -1;
      }
      if (alloc_expression_ptr(tokens, errtok, index_expr, &index_ptr) < 0) {
        free(base_ptr);
        ast_expression_free(&index_expr);
        return -1;
      }

      node->kind = AST_EXPR_LVALUE;
      node->expr.lvalue.kind = AST_LVALUE_BRACKETS;
      node->expr.lvalue.expr.brackets.base = base_ptr;
      node->expr.lvalue.expr.brackets.index = index_ptr;
      continue;
    }

    if (tok.type == TOKEN_DOT) {
      token_stream_read(tokens);
      Token member = token_stream_peek(tokens, 0);
      if (member.type != TOKEN_IDENTIFIER && member.type != TOKEN_KEYWORD) {
        set_error_token(errtok, member);
        return -1;
      }
      Token consumed_member = token_stream_read(tokens);

      char *name = dup_cstr(consumed_member.value.string);
      if (name == NULL) {
        set_static_error(errtok, consumed_member.line, consumed_member.col,
                         "out of memory");
        return -1;
      }

      AstExpression *base_ptr = NULL;
      if (alloc_expression_ptr(tokens, errtok, *node, &base_ptr) < 0) {
        free(name);
        return -1;
      }

      node->kind = AST_EXPR_LVALUE;
      node->expr.lvalue.kind = AST_LVALUE_DOT;
      node->expr.lvalue.expr.dot.base = base_ptr;
      node->expr.lvalue.expr.dot.identifier = name;
      continue;
    }

    break;
  }

  return 0;
}

static int parse_unary(TokenStream *tokens, AstExpression *node,
                       Token *errtok) {
  skip_newlines(tokens);
  Token tok = token_stream_peek(tokens, 0);

  if (tok.type == TOKEN_MINUS || tok.type == TOKEN_EXCLAMATION_MARK ||
      tok.type == TOKEN_AMPERSAND || keyword_is(tok, "mk") ||
      keyword_is(tok, "rm")) {
    Token op = token_stream_read(tokens);
    char *keyword = NULL;
    if (op.type == TOKEN_KEYWORD) {
      keyword = dup_cstr(op.value.string);
      if (keyword == NULL) {
        set_static_error(errtok, op.line, op.col, "out of memory");
        return -1;
      }
    }

    AstExpression operand = {0};
    if (parse_unary(tokens, &operand, errtok) < 0) {
      free(keyword);
      return -1;
    }

    AstExpression *operand_ptr = NULL;
    if (alloc_expression_ptr(tokens, errtok, operand, &operand_ptr) < 0) {
      ast_expression_free(&operand);
      free(keyword);
      return -1;
    }

    node->kind = AST_EXPR_UNARY;
    node->expr.unary.op = op.type;
    node->expr.unary.keyword = keyword;
    node->expr.unary.operand = operand_ptr;
    return 0;
  }

  if (parse_primary(tokens, node, errtok) < 0) {
    return -1;
  }
  if (parse_postfix(tokens, node, errtok) < 0) {
    ast_expression_free(node);
    return -1;
  }
  return 0;
}

static int parse_binary_chain(TokenStream *tokens, AstExpression *node,
                              Token *errtok,
                              int (*next_fn)(TokenStream *, AstExpression *,
                                             Token *),
                              int (*accept_op)(TokenType)) {
  AstExpression left = {0};
  if (next_fn(tokens, &left, errtok) < 0) {
    return -1;
  }

  while (1) {
    skip_newlines(tokens);
    Token op = token_stream_peek(tokens, 0);
    if (!accept_op(op.type)) {
      break;
    }
    if ((op.type == TOKEN_PLUS || op.type == TOKEN_MINUS ||
         op.type == TOKEN_ASTERISK || op.type == TOKEN_SLASH) &&
        token_stream_peek(tokens, 1).type == TOKEN_EQUALS) {
      break;
    }
    token_stream_read(tokens);

    AstExpression right = {0};
    if (next_fn(tokens, &right, errtok) < 0) {
      ast_expression_free(&left);
      return -1;
    }

    AstExpression *left_ptr = NULL;
    AstExpression *right_ptr = NULL;
    if (alloc_expression_ptr(tokens, errtok, left, &left_ptr) < 0) {
      ast_expression_free(&left);
      ast_expression_free(&right);
      return -1;
    }
    if (alloc_expression_ptr(tokens, errtok, right, &right_ptr) < 0) {
      ast_expression_free(left_ptr);
      free(left_ptr);
      ast_expression_free(&right);
      return -1;
    }

    left.kind = AST_EXPR_BINARY;
    left.expr.binary.op = op.type;
    left.expr.binary.left = left_ptr;
    left.expr.binary.right = right_ptr;
  }

  *node = left;
  return 0;
}

static int is_mul_op(TokenType type) {
  return type == TOKEN_ASTERISK || type == TOKEN_SLASH || type == TOKEN_PERCENT;
}

static int is_add_op(TokenType type) {
  return type == TOKEN_PLUS || type == TOKEN_MINUS;
}

static int is_cmp_op(TokenType type) {
  return type == TOKEN_LESS || type == TOKEN_LESS_EQUALS ||
         type == TOKEN_GREATER || type == TOKEN_GREATER_EQUALS;
}

static int is_eq_op(TokenType type) {
  return type == TOKEN_DOUBLE_EQUALS || type == TOKEN_NOT_EQUALS;
}

static int ast_expression_parse_multiplicative(TokenStream *tokens,
                                               AstExpression *node,
                                               Token *errtok) {
  return parse_binary_chain(tokens, node, errtok, parse_unary, is_mul_op);
}

static int ast_expression_parse_additive(TokenStream *tokens, AstExpression *node,
                                         Token *errtok) {
  return parse_binary_chain(tokens, node, errtok,
                            ast_expression_parse_multiplicative, is_add_op);
}

static int ast_expression_parse_comparison(TokenStream *tokens,
                                           AstExpression *node,
                                           Token *errtok) {
  return parse_binary_chain(tokens, node, errtok, ast_expression_parse_additive,
                            is_cmp_op);
}

static int ast_expression_parse_equality(TokenStream *tokens, AstExpression *node,
                                         Token *errtok) {
  return parse_binary_chain(tokens, node, errtok,
                            ast_expression_parse_comparison, is_eq_op);
}

static int is_and_op(TokenType type) {
  return type == TOKEN_AND;
}

static int is_or_op(TokenType type) {
  return type == TOKEN_OR;
}

static int ast_expression_parse_and(TokenStream *tokens, AstExpression *node,
                                     Token *errtok) {
  return parse_binary_chain(tokens, node, errtok,
                            ast_expression_parse_equality, is_and_op);
}

static int ast_expression_parse_or(TokenStream *tokens, AstExpression *node,
                                    Token *errtok) {
  return parse_binary_chain(tokens, node, errtok,
                            ast_expression_parse_and, is_or_op);
}

static int ast_expression_parse_assignment(TokenStream *tokens,
                                           AstExpression *node,
                                           Token *errtok) {
  AstExpression left = {0};
  if (ast_expression_parse_or(tokens, &left, errtok) < 0) {
    return -1;
  }

  skip_newlines(tokens);
  Token op = token_stream_peek(tokens, 0);
  Token maybe_eq = token_stream_peek(tokens, 1);

  if ((op.type == TOKEN_PLUS || op.type == TOKEN_MINUS ||
       op.type == TOKEN_ASTERISK || op.type == TOKEN_SLASH) &&
      maybe_eq.type == TOKEN_EQUALS) {
    token_stream_read(tokens);
    token_stream_read(tokens);

    if (left.kind != AST_EXPR_LVALUE) {
      set_error_token(errtok, op);
      ast_expression_free(&left);
      return -1;
    }

    AstExpression right = {0};
    if (ast_expression_parse_assignment(tokens, &right, errtok) < 0) {
      ast_expression_free(&left);
      return -1;
    }

    AstLvalueExpression *left_ptr = malloc(sizeof(*left_ptr));
    AstExpression *right_ptr = NULL;
    if (left_ptr == NULL) {
      ast_expression_free(&left);
      ast_expression_free(&right);
      set_static_error(errtok, op.line, op.col, "out of memory");
      return -1;
    }
    if (alloc_expression_ptr(tokens, errtok, right, &right_ptr) < 0) {
      free(left_ptr);
      ast_expression_free(&left);
      ast_expression_free(&right);
      return -1;
    }

    *left_ptr = left.expr.lvalue;
    node->kind = AST_EXPR_COMPOUND_ASSIGN;
    node->expr.compound_assignment.op = op.type;
    node->expr.compound_assignment.left = left_ptr;
    node->expr.compound_assignment.right = right_ptr;
    return 0;
  }

  if (op.type != TOKEN_EQUALS) {
    *node = left;
    return 0;
  }
  token_stream_read(tokens);

  if (left.kind != AST_EXPR_LVALUE) {
    set_error_token(errtok, op);
    ast_expression_free(&left);
    return -1;
  }

  AstExpression right = {0};
  if (ast_expression_parse_assignment(tokens, &right, errtok) < 0) {
    ast_expression_free(&left);
    return -1;
  }

  AstLvalueExpression *left_ptr = malloc(sizeof(*left_ptr));
  AstExpression *right_ptr = NULL;
  if (left_ptr == NULL) {
    ast_expression_free(&left);
    ast_expression_free(&right);
    set_static_error(errtok, op.line, op.col, "out of memory");
    return -1;
  }
  if (alloc_expression_ptr(tokens, errtok, right, &right_ptr) < 0) {
    free(left_ptr);
    ast_expression_free(&left);
    ast_expression_free(&right);
    return -1;
  }

  *left_ptr = left.expr.lvalue;
  node->kind = AST_EXPR_ASSIGNMENT;
  node->expr.assignment.left = left_ptr;
  node->expr.assignment.right = right_ptr;
  return 0;
}

int ast_expression_parse(TokenStream *tokens, AstExpression *node,
                         Token *errtok) {
  return ast_expression_parse_assignment(tokens, node, errtok);
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
    token_stream_read(tokens); // consume "as"
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
    token_stream_read(tokens); // consume dot
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

static void ast_lvalue_free(AstLvalueExpression *node) {
  switch (node->kind) {
  case AST_LVALUE_IDENTIFIER:
    free(node->expr.identifier);
    break;
  case AST_LVALUE_BRACKETS:
    if (node->expr.brackets.base != NULL) {
      ast_expression_free(node->expr.brackets.base);
      free(node->expr.brackets.base);
    }
    if (node->expr.brackets.index != NULL) {
      ast_expression_free(node->expr.brackets.index);
      free(node->expr.brackets.index);
    }
    break;
  case AST_LVALUE_DOT:
    if (node->expr.dot.base != NULL) {
      ast_expression_free(node->expr.dot.base);
      free(node->expr.dot.base);
    }
    free(node->expr.dot.identifier);
    break;
  }
}

static void ast_switch_clause_free(AstSwitchClause *clause) {
  if (clause->value != NULL) {
    ast_expression_free(clause->value);
    free(clause->value);
  }
  for (size_t i = 0; i < clause->len; i++) {
    ast_statement_free(&clause->statements[i]);
  }
  free(clause->statements);
  clause->value = NULL;
  clause->statements = NULL;
  clause->len = 0;
}

void ast_expression_free(AstExpression *node) {
  switch (node->kind) {
  case AST_EXPR_LITERAL:
    if (node->expr.literal.kind == AST_LITERAL_STRING) {
      free(node->expr.literal.expr.string.val);
    }
    break;
  case AST_EXPR_LVALUE:
    ast_lvalue_free(&node->expr.lvalue);
    break;
  case AST_EXPR_ASSIGNMENT:
    if (node->expr.assignment.left != NULL) {
      ast_lvalue_free(node->expr.assignment.left);
      free(node->expr.assignment.left);
    }
    if (node->expr.assignment.right != NULL) {
      ast_expression_free(node->expr.assignment.right);
      free(node->expr.assignment.right);
    }
    break;
  case AST_EXPR_COMPOUND_ASSIGN:
    if (node->expr.compound_assignment.left != NULL) {
      ast_lvalue_free(node->expr.compound_assignment.left);
      free(node->expr.compound_assignment.left);
    }
    if (node->expr.compound_assignment.right != NULL) {
      ast_expression_free(node->expr.compound_assignment.right);
      free(node->expr.compound_assignment.right);
    }
    break;
  case AST_EXPR_FUNCTION_CALL:
    if (node->expr.function_call.function != NULL) {
      ast_expression_free(node->expr.function_call.function);
      free(node->expr.function_call.function);
    }
    for (size_t i = 0; i < node->expr.function_call.arg_count; i++) {
      ast_expression_free(&node->expr.function_call.args[i]);
    }
    free(node->expr.function_call.args);
    if (node->expr.function_call.arg_names != NULL) {
      for (size_t i = 0; i < node->expr.function_call.arg_count; i++) {
        free(node->expr.function_call.arg_names[i]);
      }
      free(node->expr.function_call.arg_names);
    }
    break;
  case AST_EXPR_GROUP:
    if (node->expr.group.expr != NULL) {
      ast_expression_free(node->expr.group.expr);
      free(node->expr.group.expr);
    }
    break;
  case AST_EXPR_UNARY:
    free(node->expr.unary.keyword);
    if (node->expr.unary.operand != NULL) {
      ast_expression_free(node->expr.unary.operand);
      free(node->expr.unary.operand);
    }
    break;
  case AST_EXPR_BINARY:
    if (node->expr.binary.left != NULL) {
      ast_expression_free(node->expr.binary.left);
      free(node->expr.binary.left);
    }
    if (node->expr.binary.right != NULL) {
      ast_expression_free(node->expr.binary.right);
      free(node->expr.binary.right);
    }
    break;
  case AST_EXPR_ARRAY_LITERAL:
    for (size_t i = 0; i < node->expr.array_literal.element_count; i++) {
      ast_expression_free(&node->expr.array_literal.elements[i]);
    }
    free(node->expr.array_literal.elements);
    break;
  }
}

static void ast_block_free(AstBlock *node) {
  for (size_t i = 0; i < node->len; i++) {
    ast_statement_free(&node->statements[i]);
  }
  free(node->statements);
  node->statements = NULL;
  node->len = 0;
}

void ast_statement_free(AstStatement *node) {
  switch (node->kind) {
  case AST_STMT_VAR_DECL:
    free(node->stmt.var_decl.name);
    free(node->stmt.var_decl.type_name);
    if (node->stmt.var_decl.initializer != NULL) {
      ast_expression_free(node->stmt.var_decl.initializer);
      free(node->stmt.var_decl.initializer);
    }
    break;
  case AST_STMT_RETURN:
    if (node->stmt.return_stmt.value != NULL) {
      ast_expression_free(node->stmt.return_stmt.value);
      free(node->stmt.return_stmt.value);
    }
    break;
  case AST_STMT_IF:
    if (node->stmt.if_stmt.condition != NULL) {
      ast_expression_free(node->stmt.if_stmt.condition);
      free(node->stmt.if_stmt.condition);
    }
    if (node->stmt.if_stmt.then_block != NULL) {
      ast_block_free(node->stmt.if_stmt.then_block);
      free(node->stmt.if_stmt.then_block);
    }
    if (node->stmt.if_stmt.else_branch != NULL) {
      ast_statement_free(node->stmt.if_stmt.else_branch);
      free(node->stmt.if_stmt.else_branch);
    }
    break;
  case AST_STMT_WHILE:
    if (node->stmt.while_stmt.condition != NULL) {
      ast_expression_free(node->stmt.while_stmt.condition);
      free(node->stmt.while_stmt.condition);
    }
    if (node->stmt.while_stmt.body != NULL) {
      ast_block_free(node->stmt.while_stmt.body);
      free(node->stmt.while_stmt.body);
    }
    break;
  case AST_STMT_FOR_RANGE:
    free(node->stmt.for_stmt.name);
    if (node->stmt.for_stmt.start != NULL) {
      ast_expression_free(node->stmt.for_stmt.start);
      free(node->stmt.for_stmt.start);
    }
    if (node->stmt.for_stmt.end != NULL) {
      ast_expression_free(node->stmt.for_stmt.end);
      free(node->stmt.for_stmt.end);
    }
    if (node->stmt.for_stmt.body != NULL) {
      ast_block_free(node->stmt.for_stmt.body);
      free(node->stmt.for_stmt.body);
    }
    break;
  case AST_STMT_SWITCH:
    if (node->stmt.switch_stmt.subject != NULL) {
      ast_expression_free(node->stmt.switch_stmt.subject);
      free(node->stmt.switch_stmt.subject);
    }
    for (size_t i = 0; i < node->stmt.switch_stmt.clause_count; i++) {
      ast_switch_clause_free(&node->stmt.switch_stmt.clauses[i]);
    }
    free(node->stmt.switch_stmt.clauses);
    break;
  case AST_STMT_EXPR:
    if (node->stmt.expr_stmt.expr != NULL) {
      ast_expression_free(node->stmt.expr_stmt.expr);
      free(node->stmt.expr_stmt.expr);
    }
    break;
  case AST_STMT_BLOCK:
    ast_block_free(&node->stmt.block);
    break;
  case AST_STMT_FN_DECL:
    free(node->stmt.fn_decl.name);
    for (size_t i = 0; i < node->stmt.fn_decl.param_count; i++) {
      free(node->stmt.fn_decl.params[i]);
      if (node->stmt.fn_decl.param_types && node->stmt.fn_decl.param_types[i])
        free(node->stmt.fn_decl.param_types[i]);
    }
    free(node->stmt.fn_decl.params);
    if (node->stmt.fn_decl.param_types)
      free(node->stmt.fn_decl.param_types);
    free(node->stmt.fn_decl.return_type);
    if (node->stmt.fn_decl.body != NULL) {
      ast_block_free(node->stmt.fn_decl.body);
      free(node->stmt.fn_decl.body);
    }
    break;
  case AST_STMT_TYPE_DECL:
    for (size_t i = 0; i < node->stmt.type_decl.field_count; i++) {
      free(node->stmt.type_decl.fields[i]);
      if (node->stmt.type_decl.field_types && node->stmt.type_decl.field_types[i])
          free(node->stmt.type_decl.field_types[i]);
    }
    free(node->stmt.type_decl.fields);
    if (node->stmt.type_decl.field_types) free(node->stmt.type_decl.field_types);
    free(node->stmt.type_decl.name);
    break;
  case AST_STMT_ANNOTATION:
    free(node->stmt.annotation.value);
    if (node->stmt.annotation.alias != NULL) {
      free(node->stmt.annotation.alias);
    }
    break;
  case AST_STMT_BREAK:
  case AST_STMT_CONTINUE:
    break;
  }
}

void ast_program_free(AstProgram *node) {
  for (size_t i = 0; i < node->len; i++) {
    ast_statement_free(&node->statements[i]);
  }
  free(node->statements);
  node->statements = NULL;
  node->len = 0;
}

static void ast_lvalue_print(AstLvalueExpression *node, int depth, FILE *out);

void ast_expression_print(AstExpression *node, int depth, FILE *out) {
  switch (node->kind) {
  case AST_EXPR_LITERAL:
    print_indent(depth, out);
    if (node->expr.literal.kind == AST_LITERAL_NUMBER) {
      fprintf(out, "Literal(Number %.15g)\n", node->expr.literal.expr.number.val);
    } else if (node->expr.literal.kind == AST_LITERAL_STRING) {
      fprintf(out, "Literal(String \"%s\")\n",
              node->expr.literal.expr.string.val == NULL
                  ? ""
                  : node->expr.literal.expr.string.val);
    } else if (node->expr.literal.kind == AST_LITERAL_BOOL) {
      fprintf(out, "Literal(Bool %s)\n",
              node->expr.literal.expr.boolean.val ? "true" : "false");
    } else {
      fprintf(out, "Literal(Null)\n");
    }
    break;
  case AST_EXPR_LVALUE:
    ast_lvalue_print(&node->expr.lvalue, depth, out);
    break;
  case AST_EXPR_ASSIGNMENT:
    print_indent(depth, out);
    fprintf(out, "Assignment\n");
    print_indent(depth + 1, out);
    fprintf(out, "Left:\n");
    ast_lvalue_print(node->expr.assignment.left, depth + 2, out);
    print_indent(depth + 1, out);
    fprintf(out, "Right:\n");
    ast_expression_print(node->expr.assignment.right, depth + 2, out);
    break;
  case AST_EXPR_COMPOUND_ASSIGN:
    print_indent(depth, out);
    fprintf(out, "CompoundAssignment(%s)\n",
            token_type_name(node->expr.compound_assignment.op));
    print_indent(depth + 1, out);
    fprintf(out, "Left:\n");
    ast_lvalue_print(node->expr.compound_assignment.left, depth + 2, out);
    print_indent(depth + 1, out);
    fprintf(out, "Right:\n");
    ast_expression_print(node->expr.compound_assignment.right, depth + 2, out);
    break;
  case AST_EXPR_FUNCTION_CALL:
    print_indent(depth, out);
    fprintf(out, "FunctionCall\n");
    print_indent(depth + 1, out);
    fprintf(out, "Function:\n");
    ast_expression_print(node->expr.function_call.function, depth + 2, out);
    print_indent(depth + 1, out);
    fprintf(out, "Args(%zu):\n", node->expr.function_call.arg_count);
    for (size_t i = 0; i < node->expr.function_call.arg_count; i++) {
      if (node->expr.function_call.arg_names != NULL &&
          node->expr.function_call.arg_names[i] != NULL) {
        print_indent(depth + 2, out);
        fprintf(out, "Field(%s):\n", node->expr.function_call.arg_names[i]);
        ast_expression_print(&node->expr.function_call.args[i], depth + 3, out);
      } else {
        ast_expression_print(&node->expr.function_call.args[i], depth + 2, out);
      }
    }
    break;
  case AST_EXPR_GROUP:
    print_indent(depth, out);
    fprintf(out, "Group\n");
    ast_expression_print(node->expr.group.expr, depth + 1, out);
    break;
  case AST_EXPR_UNARY:
    print_indent(depth, out);
    if (node->expr.unary.op == TOKEN_KEYWORD) {
      fprintf(out, "Unary(%s)\n",
              node->expr.unary.keyword == NULL ? "keyword" : node->expr.unary.keyword);
    } else {
      fprintf(out, "Unary(%s)\n", token_type_name(node->expr.unary.op));
    }
    ast_expression_print(node->expr.unary.operand, depth + 1, out);
    break;
  case AST_EXPR_BINARY:
    print_indent(depth, out);
    fprintf(out, "Binary(%s)\n", token_type_name(node->expr.binary.op));
    ast_expression_print(node->expr.binary.left, depth + 1, out);
    ast_expression_print(node->expr.binary.right, depth + 1, out);
    break;
  case AST_EXPR_ARRAY_LITERAL:
    print_indent(depth, out);
    fprintf(out, "ArrayLiteral(%zu elements)\n", node->expr.array_literal.element_count);
    for (size_t i = 0; i < node->expr.array_literal.element_count; i++) {
      ast_expression_print(&node->expr.array_literal.elements[i], depth + 1, out);
    }
    break;
  }
}

static void ast_lvalue_print(AstLvalueExpression *node, int depth, FILE *out) {
  switch (node->kind) {
  case AST_LVALUE_IDENTIFIER:
    print_indent(depth, out);
    fprintf(out, "LValue(Identifier %s)\n",
            node->expr.identifier == NULL ? "" : node->expr.identifier);
    break;
  case AST_LVALUE_BRACKETS:
    print_indent(depth, out);
    fprintf(out, "LValue(Index)\n");
    print_indent(depth + 1, out);
    fprintf(out, "Base:\n");
    ast_expression_print(node->expr.brackets.base, depth + 2, out);
    print_indent(depth + 1, out);
    fprintf(out, "Index:\n");
    ast_expression_print(node->expr.brackets.index, depth + 2, out);
    break;
  case AST_LVALUE_DOT:
    print_indent(depth, out);
    fprintf(out, "LValue(Dot %s)\n",
            node->expr.dot.identifier == NULL ? "" : node->expr.dot.identifier);
    ast_expression_print(node->expr.dot.base, depth + 1, out);
    break;
  }
}

static void ast_block_print(AstBlock *node, int depth, FILE *out) {
  print_indent(depth, out);
  fprintf(out, "Block(%zu)\n", node->len);
  for (size_t i = 0; i < node->len; i++) {
    ast_statement_print(&node->statements[i], depth + 1, out);
  }
}

void ast_statement_print(AstStatement *node, int depth, FILE *out) {
  switch (node->kind) {
  case AST_STMT_VAR_DECL:
    print_indent(depth, out);
    fprintf(out, "%sDecl(%s",
            node->stmt.var_decl.is_const ? "Const" : "Var",
            node->stmt.var_decl.name == NULL ? "" : node->stmt.var_decl.name);
    if (node->stmt.var_decl.type_name != NULL) {
      fprintf(out, ": %s", node->stmt.var_decl.type_name);
    }
    fprintf(out, ")\n");
    if (node->stmt.var_decl.initializer != NULL) {
      ast_expression_print(node->stmt.var_decl.initializer, depth + 1, out);
    }
    break;
  case AST_STMT_RETURN:
    print_indent(depth, out);
    fprintf(out, "Return\n");
    if (node->stmt.return_stmt.has_value && node->stmt.return_stmt.value != NULL) {
      ast_expression_print(node->stmt.return_stmt.value, depth + 1, out);
    }
    break;
  case AST_STMT_IF:
    print_indent(depth, out);
    fprintf(out, "If\n");
    print_indent(depth + 1, out);
    fprintf(out, "Condition:\n");
    ast_expression_print(node->stmt.if_stmt.condition, depth + 2, out);
    print_indent(depth + 1, out);
    fprintf(out, "Then:\n");
    ast_block_print(node->stmt.if_stmt.then_block, depth + 2, out);
    if (node->stmt.if_stmt.else_branch != NULL) {
      print_indent(depth + 1, out);
      fprintf(out, "Else:\n");
      ast_statement_print(node->stmt.if_stmt.else_branch, depth + 2, out);
    }
    break;
  case AST_STMT_WHILE:
    print_indent(depth, out);
    fprintf(out, "While\n");
    ast_expression_print(node->stmt.while_stmt.condition, depth + 1, out);
    ast_block_print(node->stmt.while_stmt.body, depth + 1, out);
    break;
  case AST_STMT_FOR_RANGE:
    print_indent(depth, out);
    fprintf(out, "ForRange(%s)\n",
            node->stmt.for_stmt.name == NULL ? "" : node->stmt.for_stmt.name);
    print_indent(depth + 1, out);
    fprintf(out, "Start:\n");
    ast_expression_print(node->stmt.for_stmt.start, depth + 2, out);
    print_indent(depth + 1, out);
    fprintf(out, "End:\n");
    ast_expression_print(node->stmt.for_stmt.end, depth + 2, out);
    ast_block_print(node->stmt.for_stmt.body, depth + 1, out);
    break;
  case AST_STMT_SWITCH:
    print_indent(depth, out);
    fprintf(out, "Switch\n");
    print_indent(depth + 1, out);
    fprintf(out, "Subject:\n");
    ast_expression_print(node->stmt.switch_stmt.subject, depth + 2, out);
    print_indent(depth + 1, out);
    fprintf(out, "Clauses(%zu):\n", node->stmt.switch_stmt.clause_count);
    for (size_t i = 0; i < node->stmt.switch_stmt.clause_count; i++) {
      AstSwitchClause *clause = &node->stmt.switch_stmt.clauses[i];
      print_indent(depth + 2, out);
      if (clause->is_default) {
        fprintf(out, "Default:\n");
      } else {
        fprintf(out, "Case:\n");
        ast_expression_print(clause->value, depth + 3, out);
      }
      for (size_t j = 0; j < clause->len; j++) {
        ast_statement_print(&clause->statements[j], depth + 3, out);
      }
    }
    break;
  case AST_STMT_BREAK:
    print_indent(depth, out);
    fprintf(out, "Break\n");
    break;
  case AST_STMT_CONTINUE:
    print_indent(depth, out);
    fprintf(out, "Continue\n");
    break;
  case AST_STMT_EXPR:
    print_indent(depth, out);
    fprintf(out, "ExprStmt\n");
    ast_expression_print(node->stmt.expr_stmt.expr, depth + 1, out);
    break;
  case AST_STMT_BLOCK:
    ast_block_print(&node->stmt.block, depth, out);
    break;
  case AST_STMT_FN_DECL:
    print_indent(depth, out);
    fprintf(out, "FnDecl(%s%s)\n", node->stmt.fn_decl.is_export ? "export " : "",
            node->stmt.fn_decl.name == NULL ? "" : node->stmt.fn_decl.name);
    print_indent(depth + 1, out);
    fprintf(out, "Params(%zu):\n", node->stmt.fn_decl.param_count);
    for (size_t i = 0; i < node->stmt.fn_decl.param_count; i++) {
      print_indent(depth + 2, out);
      fprintf(out, "%s\n", node->stmt.fn_decl.params[i]);
    }
    if (node->stmt.fn_decl.return_type != NULL) {
      print_indent(depth + 1, out);
      fprintf(out, "ReturnType: %s\n", node->stmt.fn_decl.return_type);
    }
    ast_block_print(node->stmt.fn_decl.body, depth + 1, out);
    break;
  case AST_STMT_TYPE_DECL:
    print_indent(depth, out);
    fprintf(out, "%sDecl(%s)\n",
            node->stmt.type_decl.is_enum ? "Enum" : "Struct",
            node->stmt.type_decl.name == NULL ? "" : node->stmt.type_decl.name);
    if (!node->stmt.type_decl.is_enum && node->stmt.type_decl.field_count > 0) {
      print_indent(depth + 1, out);
      fprintf(out, "Fields(%zu):\n", node->stmt.type_decl.field_count);
      for (size_t i = 0; i < node->stmt.type_decl.field_count; i++) {
        print_indent(depth + 2, out);
        fprintf(out, "%s", node->stmt.type_decl.fields[i]);
        if (node->stmt.type_decl.field_types && node->stmt.type_decl.field_types[i]) {
            fprintf(out, ": %s", node->stmt.type_decl.field_types[i]);
        }
        fprintf(out, "\n");
      }
    }
    break;
  case AST_STMT_ANNOTATION:
    print_indent(depth, out);
    fprintf(out, "Annotation(@(%s))\n",
            node->stmt.annotation.value == NULL ? "" : node->stmt.annotation.value);
    break;
  }
}

void ast_program_print(AstProgram *node, FILE *out) {
  fprintf(out, "Program(%zu)\n", node->len);
  for (size_t i = 0; i < node->len; i++) {
    ast_statement_print(&node->statements[i], 1, out);
  }
}
