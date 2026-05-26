#ifndef TOKEN_STREAM_H
#define TOKEN_STREAM_H

#include "lexer.h"
#include "token.h"

#include "stddef.h"

#define TOKEN_STREAM_MAX_PEEK 4
// keep blog/article naming variant as an alias.
#define TOKEN_STREAM_MAX_PEAK TOKEN_STREAM_MAX_PEEK

// token stream with bounded lookahead over the lexer.
typedef struct {
  Lexer lexer;
  Token tokens[TOKEN_STREAM_MAX_PEEK];
  Token prev;
  size_t len;
} TokenStream;

void token_stream_init(TokenStream *stream, Lexer lexer);
void token_stream_free(TokenStream *stream);

// peek `n` tokens ahead without consuming.
Token token_stream_peek(TokenStream *stream, size_t n);
// consume and return the next token.
Token token_stream_read(TokenStream *stream);

#endif // TOKEN_STREAM_H
