#include "token_stream.h"

void token_stream_init(TokenStream *stream, Lexer lexer) {
  stream->lexer = lexer;
  stream->len = 0;
  stream->prev = (Token){.type = TOKEN_COUNT, .line = 0, .col = 0, .value = {0}};
}

void token_stream_free(TokenStream *stream) {
  for (size_t i = 0; i < stream->len; i++) {
    token_free(&stream->tokens[i]);
  }
  stream->len = 0;
  token_free(&stream->prev);
}

Token token_stream_peek(TokenStream *stream, size_t n) {
  if (n >= TOKEN_STREAM_MAX_PEEK) {
    return (Token){
        .type = TOKEN_ERROR,
        .line = stream->lexer.line,
        .col = stream->lexer.col,
        .value.string = "peek exceeds TOKEN_STREAM_MAX_PEEK",
    };
  }

  while (stream->len <= n) {
    stream->tokens[stream->len] = tokenize_next(&stream->lexer);
    stream->len += 1;
  }

  return stream->tokens[n];
}

Token token_stream_read(TokenStream *stream) {
  Token token;
  if (stream->len == 0) {
    token = tokenize_next(&stream->lexer);
  } else {
    token = stream->tokens[0];
    for (size_t i = 0; i < stream->len - 1; i++) {
      stream->tokens[i] = stream->tokens[i + 1];
    }
    stream->len -= 1;
  }

  token_free(&stream->prev);
  stream->prev = token;
  return token;
}
