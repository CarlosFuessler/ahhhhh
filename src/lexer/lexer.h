#ifndef LEXER_H
#define LEXER_H

#include "stddef.h"
#include "token.h"

// Lexer-Zustand zum Lesen aus einer Quellzeichenkette
typedef struct {
  const char *input;
  size_t pos;
  int line;
  int col;
} Lexer;

// Lexer-Zustand initialisieren
Lexer lexer_init(const char *input);
// Aktuelles Zeichen vorausschauen, ohne es zu konsumieren
int lexer_peek(Lexer *lexer);
// Aktuelles Zeichen lesen und vorrücken
int lexer_next(Lexer *lexer);
// Ein Token aus der Eingabe lesen
Token tokenize_next(Lexer *lexer);

// Prüfen, ob ein Bezeichner ein Schlüsselwort ist
int is_keyword(const char *str);

#endif // LEXER_H
