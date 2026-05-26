#include "token.h"

#include "stdlib.h"

// token display names for debug output
const char *token_type_names[TOKEN_COUNT] = {
    "string-literal",
    "number-literal",
    "identifier",
    "keyword",
    "open-paren",
    "close-paren",
    "open-brace",
    "close-brace",
    "open-bracket",
    "close-bracket",
    "comma",
    "colon",
    "equals",
    "double-equals",
    "not-equals",
    "less",
    "less-equals",
    "greater",
    "greater-equals",
    "plus",
    "minus",
    "asterisk",
    "slash",
    "percent",
    "ampersand",
    "at",
    "exclamation",
    "dot",
    "double-dot",
    "and",
    "or",
    "newline",
    "end-of-file",
    "error",
};

void token_free(Token *token) {
  if (token->type == TOKEN_STRING_LITERAL || token->type == TOKEN_IDENTIFIER ||
      token->type == TOKEN_KEYWORD || token->type == TOKEN_AND ||
      token->type == TOKEN_OR) {
    free(token->value.string);
    token->value.string = NULL;
  }
}

const char *token_type_name(TokenType type) {
  if (type < 0 || type >= TOKEN_COUNT) {
    return "unknown-token";
  }
  return token_type_names[type];
}
