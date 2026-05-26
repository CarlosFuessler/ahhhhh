#ifndef LEXER_H
#define LEXER_H

#include "stddef.h"
#include "token.h"

// lexer state for reading from a source string
typedef struct {
  const char *input;
  size_t pos;
  int line;
  int col;
} Lexer;

// initialize lexer state
Lexer lexer_init(const char *input);
// peek current char without consuming it
int lexer_peek(Lexer *lexer);
// read current char and advance
int lexer_next(Lexer *lexer);
// read one token from input
Token tokenize_next(Lexer *lexer);

// check if an identifier is a keyword
int is_keyword(const char *str);

#endif // LEXER_H
