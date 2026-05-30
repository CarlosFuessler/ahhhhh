#ifndef TOKEN_STREAM_H
#define TOKEN_STREAM_H

#include "lexer.h"
#include "token.h"

#include "stddef.h"

#define TOKEN_STREAM_MAX_PEEK 4
// Namensvariante blog/article als Alias beibehalten.
#define TOKEN_STREAM_MAX_PEAK TOKEN_STREAM_MAX_PEEK

// Token-Stream mit begrenztem Lookahead über den Lexer.
typedef struct {
  Lexer lexer;
  Token tokens[TOKEN_STREAM_MAX_PEEK];
  Token prev;
  size_t len;
} TokenStream;

void token_stream_init(TokenStream *stream, Lexer lexer);
void token_stream_free(TokenStream *stream);

// `n` Token vorausschauen, ohne sie zu konsumieren.
Token token_stream_peek(TokenStream *stream, size_t n);
// Das nächste Token konsumieren und zurückgeben.
Token token_stream_read(TokenStream *stream);

#endif // TOKEN_STREAM_H
