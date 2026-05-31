#ifndef TOKEN_STREAM_H
#define TOKEN_STREAM_H

#include "lexer.h"
#include "token.h"

#include "stddef.h"

#define TOKEN_STREAM_MAX_PEEK 4

// The token stream sits on top of a lexer and lets us
// peek at the next few tokens without consuming them,
// which is useful for the parser.
typedef struct {
  Lexer lexer;
  Token tokens[TOKEN_STREAM_MAX_PEEK];
  Token prev;
  size_t len;
} TokenStream;

void token_stream_init(TokenStream *stream, Lexer lexer);
void token_stream_free(TokenStream *stream);

// look at the next n-th token without consuming it
Token token_stream_peek(TokenStream *stream, size_t n);

// consume and return the next token
Token token_stream_read(TokenStream *stream);

#endif // TOKEN_STREAM_H
