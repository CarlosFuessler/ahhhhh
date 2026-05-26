#ifndef BUFFER_H
#define BUFFER_H

#include "stddef.h"

// dynamic character buffer for building strings
typedef struct {
  char *data;
  size_t len;
  size_t cap;
} CharBuffer;

// initialize an empty buffer
void buffer_init(CharBuffer *buffer);
// free buffer memory
void buffer_free(CharBuffer *buffer);
// character to the buffer
int buffer_push(CharBuffer *buffer, char ch);
// release ownership of the buffer data
char *buffer_release(CharBuffer *buffer);

#endif // BUFFER_H
