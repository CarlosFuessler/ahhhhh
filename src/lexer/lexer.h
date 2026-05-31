#ifndef LEXER_H
#define LEXER_H

#include "stddef.h"
#include "token.h"

typedef struct {
  const char *input;
  size_t pos;
  int line;
  int col;
} Lexer;

Lexer lexer_init(const char *input);

// look at next char
int lexer_peek(Lexer *lexer);

// go to next char and return it
int lexer_next(Lexer *lexer);

// gives you the next token or an error token if something went wrong
Token tokenize_next(Lexer *lexer);

// things like if, else, while etc.
int is_keyword(const char *str);

#endif // LEXER_H
