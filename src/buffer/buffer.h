#ifndef BUFFER_H
#define BUFFER_H

#include "stddef.h"

// Dynamischer Zeichenpuffer zum Erstellen von Strings
typedef struct {
  char *data;
  size_t len;
  size_t cap;
} CharBuffer;

// Einen leeren Puffer initialisieren
void buffer_init(CharBuffer *buffer);
// Pufferspeicher freigeben
void buffer_free(CharBuffer *buffer);
// Zeichen in den Puffer schreiben
int buffer_push(CharBuffer *buffer, char ch);
// Eigentümerschaft der Pufferdaten freigeben
char *buffer_release(CharBuffer *buffer);

#endif // BUFFER_H
