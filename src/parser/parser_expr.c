#include "parser.h"
#include "buffer/buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int ast_expression_parse_assignment(TokenStream *tokens, AstExpression *node, Token *errtok);
static int ast_expression_parse_multiplicative(TokenStream *tokens, AstExpression *node, Token *errtok);
static int ast_expression_parse_additive(TokenStream *tokens, AstExpression *node, Token *errtok);
static int ast_expression_parse_comparison(TokenStream *tokens, AstExpression *node, Token *errtok);
static int ast_expression_parse_equality(TokenStream *tokens, AstExpression *node, Token *errtok);
static int ast_expression_parse_and(TokenStream *tokens, AstExpression *node, Token *errtok);
static int ast_expression_parse_or(TokenStream *tokens, AstExpression *node, Token *errtok);

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
    // scan ahead to decide if this is an array literal or a type identifier hack
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
        // commas at top level of brackets mean it's definitely an array literal
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